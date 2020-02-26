/**********************************************************************
 *
 * FESTIval - Framework to Evaluate SpaTial Indices in non-VolAtiLe memories and hard disk drives.
 * https://accarniel.github.io/FESTIval/
 *
 * Copyright (C) 2016-2020 Anderson Chaves Carniel <accarniel@gmail.com>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 * Fully developed by Anderson Chaves Carniel
 *
 **********************************************************************/

/*
 * This file implements the eFIND's read buffer following the standard LRU page replacement algorithm
 * the main consideration is that the values stored in such buffer have variable sizes
 * that is, we did not consider fixed sizes for the nodes here!
 */

#include "efind_read_buffer_policies.h"
#include "efind_temporal_control.h" //for updating temporal control of reads
#include "efind_buffer_manager.h" //for the generic operations of the read buffer
#include "../libraries/uthash/uthash.h" //for hashing structures
#include "../rtree/rnode.h" //for get and put nodes for RTree structures
#include "../main/log_messages.h" //for messages

#include "../main/statistical_processing.h" //for collection of statistical data

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)

typedef struct ReadBufferLRU {
    UT_hash_handle hh;

    int page_id; //the key
    void *node; //the entries of this node (e.g., RNode pointer or HilbertRNode pointer)
} ReadBufferLRU;

static ReadBufferLRU *rb = NULL;
static size_t efind_read_buffer_size = 0; //size in bytes of our read buffer

/***
 * common functions
 ***/

static void check_if_index_is_supported(uint8_t index_type) {
    if (!(index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE || index_type == eFIND_HILBERT_RTREE_TYPE)) {
        _DEBUGF(ERROR, "eFIND does not support this index (%d) yet.", index_type);
    }
}

static void efind_readbuffer_remove_entry(ReadBufferLRU *entry, UIPage *page) {
    //page and this points to the same RNode
    HASH_DEL(rb, entry);
    efind_read_buffer_size -= efind_pagehandler_get_size(page) + sizeof (int);
    efind_pagehandler_destroy(page);
    lwfree(entry);
}

UIPage *efind_readbuffer_lru_get(const SpatialIndex * base,
        const eFINDSpecification * spec, int node_page, int height) {
    ReadBufferLRU *entry;
    int index_type;
    UIPage *ret = NULL;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif 

    index_type = spatialindex_get_type(base);

    //this function will kill the program if the underlying index is not supported
    check_if_index_is_supported(index_type);

    HASH_FIND_INT(rb, &node_page, entry);
    if (entry != NULL) {
        //_DEBUGF(NOTICE, "The node %d is in the read buffer", node_page);

        // remove it (so the subsequent add will throw it on the front of the list)
        //this happens because ut_hash stores the entries in a doubled linked list
        HASH_DEL(rb, entry);
        HASH_ADD_INT(rb, page_id, entry);

        ret = efind_pagehandler_create_clone(entry->node, index_type);

#ifdef COLLECT_STATISTICAL_DATA
        _read_buffer_page_hit++;
        cpuend = get_CPU_time();
        end = get_current_time();

        _read_buffer_get_node_cpu_time += get_elapsed_time(cpustart, cpuend);
        _read_buffer_get_node_time += get_elapsed_time(start, end);
#endif
    } else {
        /*this entry does not exist in our LRU buffer
        then we have to get this node from the storage device*/

        /*_DEBUGF(NOTICE, "The node %d is not stored in the read buffer, so "
                "lets read it from the storage device "
                " and put it on the read buffer if it has space", node_page);*/

        /* we have to read this page according to the function of the underlying index */
        if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
            ret = efind_pagehandler_create((void *) get_rnode(base, node_page, height), index_type);
        } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
            ret = efind_pagehandler_create((void*) get_hilbertnode(base, node_page, height), index_type);
        }

        /*we save it in our read temporal control, if any */
        efind_add_read_temporal_control(spec, node_page);

#ifdef COLLECT_STATISTICAL_DATA        
        //we do not take into account the put node here because it has its own time collector
        cpuend = get_CPU_time();
        end = get_current_time();

        _read_buffer_get_node_cpu_time += get_elapsed_time(cpustart, cpuend);
        _read_buffer_get_node_time += get_elapsed_time(start, end);
#endif

        /* now we check if we need to store this node in the read buffer
        we will not enforce this storage*/
        efind_readbuffer_lru_put(base, spec, ret, node_page, false);
    }

    return ret;
}

void efind_readbuffer_lru_put(const SpatialIndex *base,
        const eFINDSpecification *spec, UIPage *page, int node_page, bool mod) {
    if (spec->read_buffer_size > 0) {
        ReadBufferLRU *entry, *tmp_entry;
        UIPage *this;
        int required_size;
        uint8_t index_type;

#ifdef COLLECT_STATISTICAL_DATA
        struct timespec cpustart;
        struct timespec cpuend;
        struct timespec start;
        struct timespec end;

        cpustart = get_CPU_time();
        start = get_current_time();

        _cur_buffer_size -= efind_read_buffer_size;
#endif

        index_type = spatialindex_get_type(base);

        //this function will kill the program if the underlying index is not supported
        check_if_index_is_supported(index_type);

        //size of a node plus size of a key in the hash plus
        required_size = efind_pagehandler_get_size(page) + sizeof (int);


        if (spec->read_buffer_size < required_size) {
            /*    _DEBUGF(NOTICE, "The buffer has very low capacity (%zu) and thus, "
                        "cannot store this node (size of this node is %d)",
                        spec->read_buffer_size, required_size);*/
            return;
        }

        HASH_FIND_INT(rb, &node_page, entry);

        if (entry != NULL) {
            //this node is stored on the buffer, then we need to update it only if modifications were made
            //we also take into account the LRU management
            if (mod) {
                int diff_size = 0;

                /*_DEBUGF(NOTICE, "The node %d is in the buffer, then we may update its content",
                        node_page);*/

                this = efind_pagehandler_create(entry->node, index_type);

                diff_size = efind_pagehandler_get_size(page) - efind_pagehandler_get_size(this);

                //if there is enough space to update the information, we do it
                if (spec->read_buffer_size >= (efind_read_buffer_size + diff_size)) {
                    //_DEBUG(NOTICE, "The read buffer has space for updating the content");

                    // remove it (so the subsequent add will throw it on the front of the list)
                    HASH_DEL(rb, entry);
                    HASH_ADD_INT(rb, page_id, entry);
                    efind_pagehandler_copy_page(this, page);

                    efind_read_buffer_size += diff_size;

                    lwfree(this);
                } else {
                    /* otherwise, we must remove the old version of this element and other elements until the read buffer has enough space        */

                    /*_DEBUG(NOTICE, "There is no enough space for the updating,
                      then we remove the old version and other possible elements"); */

                    // removing the old version of this node
                    efind_readbuffer_remove_entry(entry, this);

                    HASH_ITER(hh, rb, entry, tmp_entry) {
                        //we remove other entries until we have enough space
                        if (spec->read_buffer_size >= (efind_read_buffer_size + required_size)) {
                            break;
                        } else {
                            this = efind_pagehandler_create(entry->node, index_type);
                            efind_readbuffer_remove_entry(entry, this);
                        }
                    }

                    //now we have space to put our entry!
                    entry = (ReadBufferLRU*) lwalloc(sizeof (ReadBufferLRU));
                    entry->page_id = node_page;
                    entry->node = efind_pagehandler_get_clone(page);
                    HASH_ADD_INT(rb, page_id, entry);
                    efind_read_buffer_size += required_size;
                }
            }

#ifdef COLLECT_STATISTICAL_DATA
            _read_buffer_page_hit++;
#endif
        } else {
#ifdef COLLECT_STATISTICAL_DATA
            _read_buffer_page_fault++;
#endif
            //_DEBUGF(NOTICE, "the node %d is not stored in the read buffer", node_page);
            //check if we have enough space
            if (spec->read_buffer_size < (required_size + efind_read_buffer_size)) {
                /* The read buffer does not have the enough space to put this node
                 * */

                /*_DEBUGF(NOTICE, "the node %d will be stored in the read buffer,
                thus we have to provide space for its storage", node_page);*/

                HASH_ITER(hh, rb, entry, tmp_entry) {
                    //we remove other entries until we have enough space
                    if (spec->read_buffer_size >= (efind_read_buffer_size + required_size)) {
                        break;
                    } else {
                        this = efind_pagehandler_create(entry->node, index_type);
                        efind_readbuffer_remove_entry(entry, this);
                    }
                }

                //now we have space to put our entry!
            }
            //_DEBUGF(NOTICE, "there is space to put %d in the read buffer", node_page);

            //there is space in our buffer
            entry = (ReadBufferLRU*) lwalloc(sizeof (ReadBufferLRU));
            entry->page_id = node_page;
            entry->node = efind_pagehandler_get_clone(page);
            HASH_ADD_INT(rb, page_id, entry);
            efind_read_buffer_size += required_size;
        }
#ifdef COLLECT_STATISTICAL_DATA
        cpuend = get_CPU_time();
        end = get_current_time();

        _cur_buffer_size += efind_read_buffer_size;
        _cur_read_buffer_size = efind_read_buffer_size;

        _read_buffer_put_node_cpu_time += get_elapsed_time(cpustart, cpuend);
        _read_buffer_put_node_time += get_elapsed_time(start, end);
#endif
    }
}

void efind_readbuffer_lru_update_if_needed(const SpatialIndex* base,
        const eFINDSpecification *spec, int node_page, UIPage *flushed) {
    ReadBufferLRU *entry, *entry1, *tmp_entry;
    int index_type;
    int required_size;
    UIPage *this;

    index_type = spatialindex_get_type(base);

    //this function will kill the program if the underlying index is not supported
    check_if_index_is_supported(index_type);

    HASH_FIND_INT(rb, &node_page, entry);

    if (entry != NULL) {
#ifdef COLLECT_STATISTICAL_DATA
        struct timespec cpustart;
        struct timespec cpuend;
        struct timespec start;
        struct timespec end;

        cpustart = get_CPU_time();
        start = get_current_time();

        _cur_buffer_size -= efind_read_buffer_size;
#endif
        /*_DEBUGF(NOTICE, "The node %d is in the read buffer, then we update its content if 
                space is enough for this operation",
                node_page);*/

        this = efind_pagehandler_create(entry->node, index_type);

        //size of a node plus size of a key in the hash plus size of a int that represent the height of a node
        required_size = efind_pagehandler_get_size(flushed) - efind_pagehandler_get_size(this);

        if (spec->read_buffer_size >= (efind_read_buffer_size + required_size)) {
            /*we perform the change 
             * NOTE that we did not apply the policy here since 
             *  we will only update the content of this node that was modified by
             * the flushing operation!
             */
            efind_pagehandler_copy_page(this, flushed);
            efind_read_buffer_size += required_size;
        } else {
            UIPage *t2;
            /* otherwise, we must remove the old version of this element 
             * and other elements until the read buffer has enough space */

            /*_DEBUG(NOTICE, "There is no enough space for the updating,
              then we remove the old version and other possible elements"); */

            HASH_ITER(hh, rb, entry1, tmp_entry) {
                //we remove other entries until we have enough space
                if (spec->read_buffer_size >= (efind_read_buffer_size + required_size)) {
                    break;
                } else if(entry1->page_id != entry->page_id) {
                    t2 = efind_pagehandler_create(entry1->node, index_type);
                    efind_readbuffer_remove_entry(entry1, t2);
                }
            }

            /*we perform the change 
             * NOTE that we did not apply the policy here since 
             *  we will only update the content of this node that was modified by
             * the flushing operation!
             */
            efind_pagehandler_copy_page(this, flushed);
            efind_read_buffer_size += required_size;
        }
        
        lwfree(this);
        
#ifdef COLLECT_STATISTICAL_DATA
        cpuend = get_CPU_time();
        end = get_current_time();

        _cur_buffer_size += efind_read_buffer_size;
        _cur_read_buffer_size = efind_read_buffer_size;

        _read_buffer_put_node_cpu_time += get_elapsed_time(cpustart, cpuend);
        _read_buffer_put_node_time += get_elapsed_time(start, end);
#endif
    }
}

void efind_readbuffer_lru_destroy(uint8_t index_type) {
    ReadBufferLRU *entry, *temp;
    UIPage *this;

#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size -= efind_read_buffer_size;
#endif

    HASH_ITER(hh, rb, entry, temp) {
        this = efind_pagehandler_create(entry->node, index_type);
        efind_readbuffer_remove_entry(entry, this);
    }

    //we update the size of read buffer
    efind_read_buffer_size = 0;
}

unsigned int efind_readbuffer_lru_number_of_elements() {
    return HASH_COUNT(rb);
}

