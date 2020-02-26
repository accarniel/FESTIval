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
 * This file implements the eFIND's read buffer following the full version 2Q buffer management proposed in
 * 
 * Reference: JOHNSON, T.; SHASHA, D. 2Q: A Low Overhead High Performance 
 * Buffer Management Replacement Algorithm. In Proceedings of the 
 * 20th International Conference on Very Large Data Bases (VLDB '94), p. 439-450, 1994.
 *  * 
 * the main consideration is that the values stored in such buffer have variable sizes
 * that is, we did not consider fixed sizes for the nodes here!
 */

#include "efind_read_buffer_policies.h"
#include "efind_temporal_control.h" //IMPORTANT: we use the temporal control for reads as the A1out!
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


//this corresponds to the Am part of the S2Q, which is managed as a LRU cache

typedef struct {
    UT_hash_handle hh;

    int page_id; //the key
    void *node; //the entries of this node (e.g., RNode pointer or HilbertRNode pointer)
} Am;

//this corresponds to the A1in part of the S2Q, which is managed as a FIFO cache

typedef struct {
    UT_hash_handle hh;

    int page_id; //the key
    void *node; //the entries of this node (e.g., RNode pointer or HilbertRNode pointer)
} A1in;

static Am *rb_am = NULL;
static A1in *rb_a1in = NULL;
static size_t efind_read_buffer_size = 0; //size in bytes of our read buffer (a1in + am sizes)

static size_t efind_a1in_max_size = 0;
static size_t efind_a1in_cur_size = 0;

static size_t efind_am_max_size = 0;
static size_t efind_am_cur_size = 0;

/***
 * common functions
 ***/

static void check_if_index_is_supported(uint8_t index_type) {
    if (!(index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE || index_type == eFIND_HILBERT_RTREE_TYPE)) {
        _DEBUGF(ERROR, "eFIND does not support this index (%d) yet.", index_type);
    }
}

static void efind_readbuffer_remove_entry_from_Am(Am *entry, UIPage *page) {
    //page and entry point to the same RNode
    size_t s;
    HASH_DEL(rb_am, entry);

    s = efind_pagehandler_get_size(page) + sizeof (int);
    efind_read_buffer_size -= s;
    efind_am_cur_size -= s;

    efind_pagehandler_destroy(page);
    lwfree(entry);
}

static void efind_readbuffer_remove_entry_from_A1in(A1in *entry, UIPage *page) {
    //page and entry point to the same RNode
    size_t s;
    HASH_DEL(rb_a1in, entry);

    s = efind_pagehandler_get_size(page) + sizeof (int);
    efind_read_buffer_size -= s;
    efind_a1in_cur_size -= s;

    efind_pagehandler_destroy(page);
    lwfree(entry);
}

void efind_readbuffer_2q_setsizes(const eFINDSpecification *spec, int page_size) {
    if (efind_a1in_max_size == 0 && efind_am_max_size == 0) {
        eFIND2QSpecification *s = (eFIND2QSpecification*) spec->rbp_additional_params;
        efind_a1in_max_size = spec->read_buffer_size * (s->A1in_perc_size / 100.0);

        if (efind_a1in_max_size < (page_size + sizeof (int)))
            efind_a1in_max_size = page_size + sizeof (int);

        efind_am_max_size = spec->read_buffer_size - efind_a1in_max_size;
    }
}

UIPage *efind_readbuffer_2q_get(const SpatialIndex *base,
        const eFINDSpecification *spec, int node_page, int height) {
    A1in *a1in_entry;
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

    HASH_FIND_INT(rb_a1in, &node_page, a1in_entry);
    if (a1in_entry != NULL) {
        //good! it is on the recent list, we simply return it
        ret = efind_pagehandler_create_clone(a1in_entry->node, index_type);

#ifdef COLLECT_STATISTICAL_DATA
        _read_buffer_page_hit++;
        cpuend = get_CPU_time();
        end = get_current_time();

        _read_buffer_get_node_cpu_time += get_elapsed_time(cpustart, cpuend);
        _read_buffer_get_node_time += get_elapsed_time(start, end);
#endif
        return ret;
    } else {

        //_DEBUGF(NOTICE, "The node %d is not in A1in", node_page);

        //lets check in the Am, the frequent list managed as LRU
        Am *am_entry;
        HASH_FIND_INT(rb_am, &node_page, am_entry);
        if (am_entry != NULL) {
            //good! it is on the frequent list, we simply return it and put it into the front  
            HASH_DEL(rb_am, am_entry);
            HASH_ADD_INT(rb_am, page_id, am_entry);

            ret = efind_pagehandler_create_clone(am_entry->node, index_type);

#ifdef COLLECT_STATISTICAL_DATA
            _read_buffer_page_hit++;
            cpuend = get_CPU_time();
            end = get_current_time();

            _read_buffer_get_node_cpu_time += get_elapsed_time(cpustart, cpuend);
            _read_buffer_get_node_time += get_elapsed_time(start, end);
#endif
            return ret;
        } else {
            /*this entry does not exist in Am or A1in
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

#ifdef COLLECT_STATISTICAL_DATA        
            //we do not take into account the put node here because it has its own time collector
            cpuend = get_CPU_time();
            end = get_current_time();

            _read_buffer_get_node_cpu_time += get_elapsed_time(cpustart, cpuend);
            _read_buffer_get_node_time += get_elapsed_time(start, end);
#endif
            /* now we check if we need to store this node in the read buffer
            we will not enforce this storage*/
            efind_readbuffer_2q_put(base, spec, ret, node_page, false);

            return ret;
        }
    }
}

void efind_readbuffer_2q_put(const SpatialIndex *base,
        const eFINDSpecification *spec, UIPage *page, int node_page, bool mod) {
    if (spec->read_buffer_size > 0) {
        Am *am_entry, *am_tmp_entry;
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

        //size of a node plus size of a key in the hash
        required_size = efind_pagehandler_get_size(page) + sizeof (int);

        if (spec->read_buffer_size < required_size) {
            /*    _DEBUGF(NOTICE, "The buffer has very low capacity (%zu) and thus, "
                        "cannot store this node (size of this node is %d)",
                        spec->read_buffer_size, required_size);*/
            return;
        }

        //we first check if it is not contained in the Am (managed as LRU)
        HASH_FIND_INT(rb_am, &node_page, am_entry);
        if (am_entry != NULL) {
            if (mod) {
                //this node is stored on the Am, then we need to update it according to the LRU policy
                int diff_size = 0;

                /*_DEBUGF(NOTICE, "The node %d is in the buffer, then we may update its content",
                        node_page);*/

                this = efind_pagehandler_create(am_entry->node, index_type);

                diff_size = efind_pagehandler_get_size(page) - efind_pagehandler_get_size(this);

                //if there is enough space to update the information, we do it
                if (efind_am_max_size >= (efind_am_cur_size + diff_size)) {
                    //_DEBUG(NOTICE, "The read buffer has space for updating the content");

                    // remove it (so the subsequent add will throw it on the front of the list)
                    HASH_DEL(rb_am, am_entry);
                    HASH_ADD_INT(rb_am, page_id, am_entry);

                    efind_pagehandler_copy_page(this, page);

                    efind_read_buffer_size += diff_size;
                    efind_am_cur_size += diff_size;

                    lwfree(this);
                } else {
                    /*_DEBUG(NOTICE, "There is no enough space for the updating,
                      then we remove the old version and other possible elements"); */

                    // removing the old version of this node
                    efind_readbuffer_remove_entry_from_Am(am_entry, this);

                    HASH_ITER(hh, rb_am, am_entry, am_tmp_entry) {
                        //we remove other entries until we have enough space
                        if (efind_am_max_size >= (efind_am_cur_size + required_size)) {
                            break;
                        } else {
                            this = efind_pagehandler_create(am_entry->node, index_type);
                            efind_readbuffer_remove_entry_from_Am(am_entry, this);
                        }
                    }

                    //now we have space to put our entry!
                    am_entry = (Am*) lwalloc(sizeof (Am));
                    am_entry->page_id = node_page;
                    am_entry->node = efind_pagehandler_get_clone(page);
                    HASH_ADD_INT(rb_am, page_id, am_entry);
                    efind_read_buffer_size += required_size;
                    efind_am_cur_size += required_size;
                }
            }
#ifdef COLLECT_STATISTICAL_DATA
            _read_buffer_page_hit++;
#endif
        } else {
            //we need to check if it is not contained in the A1in (managed as FIFO)
            A1in *a1in_entry, *a1in_tmp_entry;
            HASH_FIND_INT(rb_a1in, &node_page, a1in_entry);
            if (a1in_entry != NULL) {
                if (mod) {
                    //this node is stored on the A1in, then we need to update its content only
                    int diff_size = 0;

                    /*_DEBUGF(NOTICE, "The node %d is in the buffer, then we may update its content",
                            node_page);*/

                    this = efind_pagehandler_create(a1in_entry->node, index_type);

                    diff_size = efind_pagehandler_get_size(page) - efind_pagehandler_get_size(this);

                    //if there is enough space to update the information, we do it
                    if (efind_a1in_max_size >= (efind_a1in_cur_size + diff_size)) {
                        //_DEBUG(NOTICE, "The read buffer has space for updating the content");
                        efind_pagehandler_copy_page(this, page);

                        efind_read_buffer_size += diff_size;
                        efind_a1in_cur_size += diff_size;

                        lwfree(this);
                    } else {
                        /*_DEBUG(NOTICE, "There is no enough space for the updating,
                          then we remove the old version and other possible elements"); */

                        // removing the old version of this node
                        efind_readbuffer_remove_entry_from_A1in(rb_a1in, this);

                        HASH_ITER(hh, rb_a1in, a1in_entry, a1in_tmp_entry) {
                            //we remove other entries until we have enough space
                            if (efind_a1in_max_size >= (efind_a1in_cur_size + required_size)) {
                                break;
                            } else {
                                this = efind_pagehandler_create(a1in_entry->node, index_type);
                                efind_readbuffer_remove_entry_from_A1in(a1in_entry, this);
                            }
                        }

                        //now we have space to put our entry!
                        a1in_entry = (A1in*) lwalloc(sizeof (A1in));
                        a1in_entry->page_id = node_page;
                        a1in_entry->node = efind_pagehandler_get_clone(page);
                        HASH_ADD_INT(rb_a1in, page_id, a1in_entry);
                        efind_read_buffer_size += required_size;
                        efind_a1in_cur_size += required_size;
                    }
                }
#ifdef COLLECT_STATISTICAL_DATA
                _read_buffer_page_hit++;
#endif
            } else {
                /*this page is not in Am and A1in, let's check if it on the 'ghost' list, A1out
                 * this list corresponds to the read_temporal_control list*
                 */
                //we will only store it on Am if this page is contained on A1out
                uint8_t check = efind_read_temporal_control_contains(spec, node_page);

#ifdef COLLECT_STATISTICAL_DATA
                _read_buffer_page_fault++;
#endif 

                if (check == INSERTED) { //ok, we should store it on the Am          
                    //check if we have enough space
                    if (efind_am_max_size < (required_size + efind_am_cur_size)) {

                        /*_DEBUGF(NOTICE, "the node %d will be stored in the read buffer,
                        thus we have to provide space for its storage", node_page);*/

                        HASH_ITER(hh, rb_am, am_entry, am_tmp_entry) {
                            //we remove other entries until we have enough space
                            if (efind_am_max_size >= (efind_am_cur_size + required_size)) {
                                break;
                            } else {
                                this = efind_pagehandler_create(am_entry->node, index_type);
                                efind_readbuffer_remove_entry_from_Am(am_entry, this);
                            }
                        }

                        //now we have space to put our entry!

                    }
                    //we have enough space
                    //we remove this page from A1out and put it on the front of Am
                    efind_read_temporal_control_remove(spec, node_page);

                    //_DEBUGF(NOTICE, "there is space to put %d in the read buffer", node_page);

                    //there is space in our buffer
                    am_entry = (Am*) lwalloc(sizeof (Am));
                    am_entry->page_id = node_page;
                    am_entry->node = efind_pagehandler_get_clone(page);
                    HASH_ADD_INT(rb_am, page_id, am_entry);
                    efind_read_buffer_size += required_size;
                    efind_am_cur_size += required_size;
                } else {
                    //this page is not contained in ghost, recent or frequent lists
                    //we check if A1in have enough space
                    if (efind_a1in_max_size < (efind_a1in_cur_size + required_size)) {
                        //in negative case, we remove some entries from A1in, respecting the FIFO, putting them into the 'ghost' list, A1out
                        //which will handle correctly the size

                        //it will remove the elements until space is sufficient for this node

                        HASH_ITER(hh, rb_a1in, a1in_entry, a1in_tmp_entry) {
                            //we remove other entries until we have enough space
                            if (efind_a1in_max_size >= (efind_a1in_cur_size + required_size)) {
                                break;
                            } else {
                                efind_add_read_temporal_control(spec, a1in_entry->page_id);

                                this = efind_pagehandler_create(a1in_entry->node, index_type);
                                efind_readbuffer_remove_entry_from_A1in(a1in_entry, this);
                            }
                        }

                    }
                    //we have enough space in A1in
                    a1in_entry = (A1in*) lwalloc(sizeof (A1in));
                    a1in_entry->page_id = node_page;
                    a1in_entry->node = efind_pagehandler_get_clone(page);
                    HASH_ADD_INT(rb_a1in, page_id, a1in_entry);
                    efind_read_buffer_size += required_size;
                    efind_a1in_cur_size += required_size;
                }

            }
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

void efind_readbuffer_2q_update_if_needed(const SpatialIndex* base, 
        int node_page, UIPage *flushed) {
    Am *am_entry, *am_entry1, *am_tmp_entry;
    A1in *a1in_entry, *a1in_entry1, *a1in_tmp_entry;
    int index_type;
    int required_size;
    UIPage *this;

    index_type = spatialindex_get_type(base);

    //this function will kill the program if the underlying index is not supported
    check_if_index_is_supported(index_type);

    HASH_FIND_INT(rb_am, &node_page, am_entry);

    if (am_entry != NULL) {
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

        this = efind_pagehandler_create(am_entry->node, index_type);

        //size of a node plus size of a key in the hash plus size of a int that represent the height of a node
        required_size = efind_pagehandler_get_size(flushed) - efind_pagehandler_get_size(this);

        if (efind_am_max_size >= (efind_am_cur_size + required_size)) {
            /*we perform the change 
             * NOTE that we did not apply the policy here since 
             *  we will only update the content of this node that was modified by
             * the flushing operation!
             */
            efind_pagehandler_copy_page(this, flushed);
            efind_read_buffer_size += required_size;
            efind_am_cur_size += required_size;

            lwfree(this);
        } else {
            UIPage *t2;

            /*_DEBUG(NOTICE, "We don't have space, then should provide space for this update");*/
            HASH_ITER(hh, rb_am, am_entry1, am_tmp_entry) {
                //we remove other entries until we have enough space
                if (efind_am_max_size >= (efind_am_cur_size + required_size)) {
                    break;
                } else if (am_entry1->page_id != am_entry->page_id) {
                    t2 = efind_pagehandler_create(am_entry1->node, index_type);
                    efind_readbuffer_remove_entry_from_Am(am_entry1, t2);
                }
            }
            /*we perform the change 
             * NOTE that we did not apply the policy here since 
             *  we will only update the content of this node that was modified by
             * the flushing operation!
             */
            efind_pagehandler_copy_page(this, flushed);
            efind_read_buffer_size += required_size;
            efind_am_cur_size += required_size;

            lwfree(this);

        }
#ifdef COLLECT_STATISTICAL_DATA
        cpuend = get_CPU_time();
        end = get_current_time();

        _cur_buffer_size += efind_read_buffer_size;
        _cur_read_buffer_size = efind_read_buffer_size;

        _read_buffer_put_node_cpu_time += get_elapsed_time(cpustart, cpuend);
        _read_buffer_put_node_time += get_elapsed_time(start, end);
#endif
    } else {
        //we check if it is not contained on A1in
        HASH_FIND_INT(rb_a1in, &node_page, a1in_entry);
        if (a1in_entry != NULL) {
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

            this = efind_pagehandler_create(a1in_entry->node, index_type);

            //size of a node plus size of a key in the hash plus size of a int that represent the height of a node
            required_size = efind_pagehandler_get_size(flushed) - efind_pagehandler_get_size(this);

            if (efind_a1in_max_size >= (efind_a1in_cur_size + required_size)) {
                /*we perform the change 
                 * NOTE that we did not apply the policy here since 
                 *  we will only update the content of this node that was modified by
                 * the flushing operation!
                 */
                efind_pagehandler_copy_page(this, flushed);
                efind_read_buffer_size += required_size;
                efind_a1in_cur_size += required_size;

                lwfree(this);
            } else {
                UIPage *t2;

                /*_DEBUG(NOTICE, "We don't have space, then should provide space for this update");*/
                HASH_ITER(hh, rb_a1in, a1in_entry1, a1in_tmp_entry) {
                    //we remove other entries until we have enough space
                    if (efind_a1in_max_size >= (efind_a1in_cur_size + required_size)) {
                        break;
                    } else if (a1in_entry1->page_id != a1in_entry->page_id) {
                        t2 = efind_pagehandler_create(a1in_entry1->node, index_type);
                        efind_readbuffer_remove_entry_from_A1in(a1in_entry1, t2);
                    }
                }
                /*we perform the change 
                 * NOTE that we did not apply the policy here since 
                 *  we will only update the content of this node that was modified by
                 * the flushing operation!
                 */
                efind_pagehandler_copy_page(this, flushed);
                efind_read_buffer_size += required_size;
                efind_a1in_cur_size += required_size;

                lwfree(this);
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
}

void efind_readbuffer_2q_destroy(uint8_t index_type) {
    Am *am_entry, *am_temp;
    A1in *a1in_entry, *a1in_temp;
    UIPage *this;

#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size -= efind_read_buffer_size;
#endif

    HASH_ITER(hh, rb_am, am_entry, am_temp) {
        this = efind_pagehandler_create(am_entry->node, index_type);
        efind_readbuffer_remove_entry_from_Am(am_entry, this);
    }

    HASH_ITER(hh, rb_a1in, a1in_entry, a1in_temp) {
        this = efind_pagehandler_create(a1in_entry->node, index_type);
        efind_readbuffer_remove_entry_from_A1in(a1in_entry, this);
    }

    //we update the size of read buffer
    efind_read_buffer_size = 0;
    efind_am_cur_size = 0;
    efind_a1in_cur_size = 0;
}

unsigned int efind_readbuffer_2q_number_of_elements() {
    return HASH_COUNT(rb_am) + HASH_COUNT(rb_a1in);
}
