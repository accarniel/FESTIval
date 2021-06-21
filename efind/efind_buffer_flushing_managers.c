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
 * This file implements the eFIND's write buffer and its corresponding operations, as well as the flushing operation * 
 * the main consideration is that the values stored in such buffer have variable sizes
 * that is, we did not consider fixed sizes for the nodes here!
 */

#include <liblwgeom.h> //for geometry functions
#include <float.h> //to the maximum float number
#include <stringbuffer.h> //to print/debug strings as needed

#include "../main/log_messages.h" //for messages

#include "efind_buffer_manager.h" //basic operations
#include "efind_read_buffer_policies.h" //for the read buffer policy implementations
#include "efind_flushing_manager.h" //for the flushing operation
#include "efind_temporal_control.h" //for the temporal control
#include "efind_log_manager.h" //for data durability
#include "efind_mod_handler.h" //for handle the modifications (red-black tree)

#include "../libraries/uthash/uthash.h" //for hashing structures
#include "../main/statistical_processing.h" //for collection of statistical data
#include "../main/storage_handler.h" //to write the flushed nodes
#include "../main/math_util.h" //for double comparators
#include "../main/io_handler.h" //for the type of access of a file 

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)

/*our buffer is a hash map - managed by the UTHASH library*/
typedef struct WriteBuffer {
    UT_hash_handle hh;

    int page_id; //it is the node page and the hash key
    int modify_count; //number of modifications, which is the numerator of our formula
    /*the height of the node, which is used as a weight */
    int node_height;
    /*timestamp in ms of the last modification of the node*/
    int timestamp_ms;

    uint8_t status; //new, mod, or del
    eFIND_RB_Tree rb_tree; //this is invalid when status is DEL    
} WriteBuffer;

/* variables for the buffer management */
static size_t efind_write_buffer_size = 0; //size in bytes of our write buffer
static WriteBuffer *wb = NULL;

/*this function check if a flushed node is contained in the read buffer
 if this is the case, we check if we can store its new content
 otherwise, we remove the old version from the read buffer*/
static inline void efind_check_needed_update_in_readbuffer(const SpatialIndex* base, const eFINDSpecification *spec,
        int node_page, int height, UIPage *flushed);

void efind_put_node_in_readbuffer(const SpatialIndex* base, const eFINDSpecification *spec,
        UIPage *node, int node_page, int height, bool force) {
    if (spec->read_buffer_policy == eFIND_LRU_RBP) {
        efind_readbuffer_lru_put(base, spec, node, node_page, force);
    } else if (spec->read_buffer_policy == eFIND_HLRU_RBP) {
        efind_readbuffer_hlru_put(base, spec, node, node_page, height, force);
    } else if (spec->read_buffer_policy == eFIND_S2Q_RBP) {
        efind_readbuffer_s2q_put(base, spec, node, node_page, force);
    } else if (spec->read_buffer_policy == eFIND_2Q_RBP) {
        efind_readbuffer_2q_put(base, spec, node, node_page, force);
    } else {
        _DEBUGF(ERROR, "The policy (%d) is not valid for the read buffer.", spec->read_buffer_policy);
    }
}

UIPage *efind_get_node_from_readbuffer(const SpatialIndex* base, const eFINDSpecification *spec,
        int node_page, int height) {
    if (spec->read_buffer_policy == eFIND_NONE_RBP) {
        //we should read this node directly from the storage device
        UIPage *ret = NULL;
        uint8_t index_type = spatialindex_get_type(base);
        /* we have to read this page according to the function of the underlying index */
        if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
            ret = efind_pagehandler_create((void *) get_rnode(base, node_page, height), index_type);
        } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
            ret = efind_pagehandler_create((void*) get_hilbertnode(base, node_page, height), index_type);
        }
        return ret;
    } else if (spec->read_buffer_policy == eFIND_LRU_RBP) {
        return efind_readbuffer_lru_get(base, spec, node_page, height);
    } else if (spec->read_buffer_policy == eFIND_HLRU_RBP) {
        return efind_readbuffer_hlru_get(base, spec, node_page, height);
    } else if (spec->read_buffer_policy == eFIND_S2Q_RBP) {
        return efind_readbuffer_s2q_get(base, spec, node_page, height);
    } else if (spec->read_buffer_policy == eFIND_2Q_RBP) {
        return efind_readbuffer_2q_get(base, spec, node_page, height);
    } else {
        _DEBUGF(ERROR, "The policy (%d) is not valid for the read buffer.", spec->read_buffer_policy);
    }
    return NULL;
}

void efind_check_needed_update_in_readbuffer(const SpatialIndex* base, const eFINDSpecification* spec,
        int node_page, int height, UIPage* flushed) {
    if (spec->read_buffer_policy == eFIND_LRU_RBP) {
        efind_readbuffer_lru_update_if_needed(base, spec, node_page, flushed);
    } else if (spec->read_buffer_policy == eFIND_HLRU_RBP) {
        efind_readbuffer_hlru_update_if_needed(base, spec, node_page, height, flushed);
    } else if (spec->read_buffer_policy == eFIND_S2Q_RBP) {
        efind_readbuffer_s2q_update_if_needed(base, spec, node_page, flushed);
    } else if (spec->read_buffer_policy == eFIND_2Q_RBP) {
        efind_readbuffer_2q_update_if_needed(base, node_page, flushed);
    } else if (spec->read_buffer_policy != eFIND_NONE_RBP) {
        _DEBUGF(ERROR, "The policy (%d) is not valid for the read buffer.", spec->read_buffer_policy);
    }
}

unsigned int efind_readbuffer_number_of_elements(const eFINDSpecification *spec) {
    if (spec->read_buffer_policy == eFIND_LRU_RBP) {
        return efind_readbuffer_lru_number_of_elements();
    } else if (spec->read_buffer_policy == eFIND_HLRU_RBP) {
        return efind_readbuffer_hlru_number_of_elements();
    } else if (spec->read_buffer_policy == eFIND_S2Q_RBP) {
        return efind_readbuffer_s2q_number_of_elements();
    } else if (spec->read_buffer_policy == eFIND_2Q_RBP) {
        return efind_readbuffer_2q_number_of_elements();
    } else {
        _DEBUGF(ERROR, "The policy (%d) is not valid for the read buffer.", spec->read_buffer_policy);
    }
    return 0;
}

/* functions to calculate the size of the elements in the write buffer */
static size_t efind_size_of_create_entry_hash(void);
static size_t efind_size_of_del_node(void);

static void efind_free_hashvalue(int node_page, uint8_t index_type);

size_t efind_size_of_create_entry_hash() {
    //size of the hash key, status, number of modifications
    //and height, timestamp
    //and red-black tree 
    return sizeof (int) + sizeof (uint8_t) + sizeof (int) + sizeof (int)
            + sizeof (int) + sizeof (eFIND_RB_Tree);
}

size_t efind_size_of_del_node() {
    return 0;
}

unsigned int efind_writebuffer_number_of_elements() {
    return HASH_COUNT(wb);
}

/*only create a new rnode in the buffer - this node has no modifications!*/
void efind_buf_create_node(const SpatialIndex *base, eFINDSpecification *spec,
        int new_node_page, int height) {
    WriteBuffer *buf_entry;
    size_t required_size = 0;
    struct timespec tim;
    HASH_FIND_INT(wb, &new_node_page, buf_entry); //is new_node_page already in the hash?

#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size -= efind_write_buffer_size;
#endif

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = efind_size_of_create_entry_hash();
    } else {
        //if this node was previously removed, then we recreate it
        if (buf_entry->status == eFIND_STATUS_DEL) {
            required_size = 0;
#ifdef COLLECT_STATISTICAL_DATA
            _cur_del_node_buffer_num--;
#endif
        } else {
            _DEBUGF(ERROR, "This node (%d) already exists in the update node table!"
                    " Therefore, this is an invalid operation.",
                    new_node_page);
            return;
        }
    }
    //if we do not have space, we execute the flushing
    if (required_size > 0 && spec->write_buffer_size < (required_size + efind_write_buffer_size)) {

        //_DEBUG(NOTICE, "Performing flushing");

        efind_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(wb, &new_node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = efind_size_of_create_entry_hash();
        }

        //_DEBUG(NOTICE, "Flushing completed");
    }

    //_DEBUG(NOTICE, "Creating new node...");

    /*then we put it into our buffer because we have space now =)*/
    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (WriteBuffer*) lwalloc(sizeof (WriteBuffer));
        buf_entry->page_id = new_node_page;
        buf_entry->modify_count = 0;
        buf_entry->node_height = height;
        HASH_ADD_INT(wb, page_id, buf_entry); //we add it         
    }

    clock_gettime(CLOCK_MONOTONIC, &tim);
    buf_entry->timestamp_ms = (int) 1e3 * tim.tv_sec + tim.tv_nsec * 1e-6;
    buf_entry->status = eFIND_STATUS_NEW;
    buf_entry->rb_tree = RB_ROOT; //initialization of the red-black tree
    buf_entry->modify_count++;

    //we increment the buffer_size
    efind_write_buffer_size += required_size;

    //_DEBUG(NOTICE, "Registering NEW in the log");

    efind_write_log_create_node(base, spec, new_node_page, height);

    //_DEBUG(NOTICE, "Done");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_new_node_buffer_num++;
    _new_node_buffer_num++;

    _cur_buffer_size += efind_write_buffer_size;
#endif
}

/*put a modification of an existing node (this can be any node, RNode, Hilbert node, and so on)*/
void efind_buf_mod_node(const SpatialIndex *base, eFINDSpecification *spec,
        int node_page, void *entry, int height) {
    WriteBuffer *buf_entry;
    size_t max_required_size = 0;
    size_t occupated_size = 0;
    struct timespec tim;
    uint8_t index_type = spatialindex_get_type(base);
    UIEntry *this;
    eFIND_Modification *mod;

    this = efind_entryhandler_create(entry, index_type, &height);

    HASH_FIND_INT(wb, &node_page, buf_entry); //is new_node_page already in the hash?

#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size -= efind_write_buffer_size;
#endif

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        max_required_size = efind_size_of_create_entry_hash() +
                efind_entryhandler_size(this) + sizeof (eFIND_Modification);
    } else {
        //if this node was previously removed, then we must recreate it
        if (buf_entry->status == eFIND_STATUS_DEL) {
            _DEBUG(ERROR, "Invalid operation! You are trying to put an element in a removed node!");
            return;
        } else {
            max_required_size = efind_entryhandler_size(this) + sizeof (eFIND_Modification);
        }
    }
    //if we do not have space, we execute the flushing
    if (spec->write_buffer_size < (max_required_size + efind_write_buffer_size)) {
        //_DEBUG(NOTICE, "Performing flushing");

        efind_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(wb, &node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            occupated_size = efind_size_of_create_entry_hash();
        }

        //_DEBUG(NOTICE, "Flushing completed");
    } else if (buf_entry == NULL) {
        occupated_size = efind_size_of_create_entry_hash();
    }

    lwfree(this);

    /*then we put it into our buffer because we have space now =)*/
    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (WriteBuffer*) lwalloc(sizeof (WriteBuffer));
        buf_entry->page_id = node_page;
        buf_entry->modify_count = 0;
        buf_entry->node_height = height;
        buf_entry->status = eFIND_STATUS_MOD;
        buf_entry->rb_tree = RB_ROOT;
        HASH_ADD_INT(wb, page_id, buf_entry); //we add it  
    }

    clock_gettime(CLOCK_MONOTONIC, &tim);
    buf_entry->timestamp_ms = (int) 1e3 * tim.tv_sec + tim.tv_nsec * 1e-6;

    //_DEBUG(NOTICE, "Inserting the MOD in the buffer");

    //we add the modification in our red-black tree
    mod = (eFIND_Modification*) lwalloc(sizeof (eFIND_Modification));
    mod->entry = entry;
    occupated_size += efind_writebuffer_add_mod(&buf_entry->rb_tree, mod, index_type, height);
    buf_entry->modify_count++;

    //we increment the buffer_size
    efind_write_buffer_size += occupated_size;

    //_DEBUG(NOTICE, "Registering MOD in the log");

    efind_write_log_mod_node(base, spec, node_page, entry, height);

    //_DEBUG(NOTICE, "Done");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_mod_node_buffer_num++;
    _mod_node_buffer_num++;

    _cur_buffer_size += efind_write_buffer_size;
#endif
}

/*delete a rnode (which can be stored in the disk or not)*/
void efind_buf_del_node(const SpatialIndex *base, eFINDSpecification *spec,
        int node_page, int height) {
    WriteBuffer *buf_entry;
    long int required_size = 0;
    struct timespec tim;
    uint8_t index_type = spatialindex_get_type(base);
    HASH_FIND_INT(wb, &node_page, buf_entry); //is new_node_page already in the hash?

#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size -= efind_write_buffer_size;
#endif

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = efind_size_of_create_entry_hash() + efind_size_of_del_node();
    } else {
        required_size = efind_size_of_del_node();
    }
    //if we do not have space, we execute the flushing
    if (required_size > 0 && spec->write_buffer_size < (required_size + efind_write_buffer_size)) {
        //_DEBUG(NOTICE, "Performing flushing");

        efind_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(wb, &node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = efind_size_of_create_entry_hash() + efind_size_of_del_node();
        }

        //_DEBUG(NOTICE, "Flushing completed");
    }

    //_DEBUGF(NOTICE, "Removing node %d from the buffer", node_page);

    /*then we put it into our buffer because we have space now =)*/
    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (WriteBuffer*) lwalloc(sizeof (WriteBuffer));
        buf_entry->page_id = node_page;
        buf_entry->status = eFIND_STATUS_DEL;
        buf_entry->rb_tree = RB_ROOT;
        buf_entry->modify_count = 0;
        buf_entry->node_height = height;
        HASH_ADD_INT(wb, page_id, buf_entry); //we add it         
    } else {
#ifdef COLLECT_STATISTICAL_DATA
        if (buf_entry->status == eFIND_STATUS_NEW) {
            _cur_new_node_buffer_num--;
        }
#endif
        //we need to del the modification tree
        //as a consequence we will have some new space
        buf_entry->status = eFIND_STATUS_DEL;

        //we will free some space from buffer here        
        required_size -= efind_writebuffer_destroy_mods(&buf_entry->rb_tree, index_type, height);
    }

    clock_gettime(CLOCK_MONOTONIC, &tim);
    buf_entry->timestamp_ms = (int) 1e3 * tim.tv_sec + tim.tv_nsec * 1e-6;
    buf_entry->modify_count++;

    //we increment the forb_buffer_size
    efind_write_buffer_size += required_size;

    //_DEBUG(NOTICE, "Registering DEL in the log");

    efind_write_log_del_node(base, spec, node_page, height);

    //_DEBUG(NOTICE, "Done");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_del_node_buffer_num++;
    _del_node_buffer_num++;

    _cur_buffer_size += efind_write_buffer_size;
#endif
}

/*we retrieve the most recent version of a node by considering possible modification in the buffer
 after the call, we return the most recent version of the request node (rnode_page)
 it returns a void pointer to the node structure that the index is handling
 e.g., an RNode for Rtree indices*/
void *efind_buf_retrieve_node(const SpatialIndex *base, const eFINDSpecification *spec,
        int node_page, int height) {
    WriteBuffer *buf_entry;
    UIPage *page_ret = NULL; //page to be returned
    UIPage *page_ss = NULL; //page stored in the storage system
    void *ret = NULL; //void pointer for the page inside page_ret, which is effectively used by an index
    uint8_t index_type;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    index_type = spatialindex_get_type(base);

    //_DEBUG(NOTICE, "Retrieving a node");

    HASH_FIND_INT(wb, &node_page, buf_entry); //is rnode_page already in the hash?
    if (buf_entry != NULL) {
        //if this node is in the buffer, we check if it is a newly created node
        //or a node with modifications
        if (buf_entry->status == eFIND_STATUS_MOD ||
                buf_entry->status == eFIND_STATUS_NEW) {
            //if it is a modified node, we need to apply the modification list
            if (buf_entry->status == eFIND_STATUS_MOD) {
                //_DEBUG(NOTICE, "This node has modifications, which means that we should merge");
                //we get this node from the read buffer
                //if the read buffer does not contain this node
                //the read buffer is responsible to get it from the storage device                
                page_ss = efind_get_node_from_readbuffer(base, spec, node_page, height);
            }

            //performs the merging operation between the modifications stored in the buffer with the entries stored in the storage device
            page_ret = efind_writebuffer_merge_mods(&buf_entry->rb_tree, page_ss, index_type, height);

            //free page_ss since it is a clone
            if (page_ss != NULL)
                efind_pagehandler_destroy(page_ss);
        } else {
            //this node does not exist... we have an error if we aren't retrieving this node for flushing operation
            //since it should not happen because other nodes should not point to this deleted node!
            return NULL;
        }
    } else {
        //this node is not in the buffer
        //therefore, we have to return the node stored from the read buffer
        //the read buffer is responsible to get it from the storage device                
        page_ret = efind_get_node_from_readbuffer(base, spec, node_page, height);
    }

    ret = efind_pagehandler_get(page_ret);
    lwfree(page_ret);

    /*
    if (buf_entry == NULL) {
        _DEBUG(NOTICE, "Retrieving node from storage device completed :");
        hilbertnode_print((HilbertRNode*) ret, node_page);

        _DEBUG(ERROR, "CHECK");
    } else {
        if (buf_entry->status == eFIND_STATUS_NEW) {
            int n = ((HilbertRNode*) ret)->nofentries;
            if (n > 5) {
                _DEBUG(NOTICE, "Retrieving node from write buffer completed :");
                hilbertnode_print((HilbertRNode*) ret, node_page);

                _DEBUG(ERROR, "CHECK");
            }
        } else {
            _DEBUG(NOTICE, "Retrieving node from the merge completed :");
            hilbertnode_print((HilbertRNode*) ret, node_page);

            _DEBUG(ERROR, "CHECK");
        }
    }*/

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    if (_STORING == 0) {
        _ret_node_from_buf_cpu_time += get_elapsed_time(cpustart, cpuend);
        _ret_node_from_buf_time += get_elapsed_time(start, end);
    }
#endif

    return ret;
}

void efind_free_hashvalue(int node_page, uint8_t index_type) {
    WriteBuffer *buf_entry;
    size_t removed_size = 0;

    HASH_FIND_INT(wb, &node_page, buf_entry); //is rnode_page already in the hash?
    if (buf_entry != NULL) {
        HASH_DEL(wb, buf_entry);
        removed_size += efind_size_of_create_entry_hash();

        //we will free some space from buffer here    
        if (buf_entry->status == eFIND_STATUS_MOD ||
                buf_entry->status == eFIND_STATUS_NEW)
            removed_size += efind_writebuffer_destroy_mods(&buf_entry->rb_tree, index_type, buf_entry->node_height);

#ifdef COLLECT_STATISTICAL_DATA
        if (buf_entry->status == eFIND_STATUS_NEW) {
            _cur_new_node_buffer_num--;
            //we decrement the number of modification less one (since one represent that the node when created)
            _cur_mod_node_buffer_num -= buf_entry->modify_count - 1;
        } else if (buf_entry->status == eFIND_STATUS_DEL) {
            _cur_del_node_buffer_num--;
            //we decrement the number of modification less one (since one represent that the node when removed)
            _cur_mod_node_buffer_num -= buf_entry->modify_count - 1;
        } else if (buf_entry->status == eFIND_STATUS_MOD) {
            //TODO This value is calculated incorrectly since a node can be removed and inserted several times before a flushing operation
            _cur_mod_node_buffer_num -= buf_entry->modify_count;
        }
        //we do not update the value of the _cur_buffer_size here because
        //this function is called inside the flushing operation
#endif
        lwfree(buf_entry);
    } else {
        _nof_unnecessary_flushed_nodes++;
        //_DEBUG(WARNING, "We cannot free a node that do not exist in the hash table..."
        //        "Probably the flushing module has written a node that was not needed into disk");
    }
    //we decrement the removed_size from the buffer size
    efind_write_buffer_size -= removed_size;
}

/*sorting comparators*/
static int page_id_sort(WriteBuffer *a, WriteBuffer *b);
static int timestamp_sort(WriteBuffer *a, WriteBuffer *b);
static int nodes_to_flush_comp(const void * elem1, const void * elem2);

//used in the flush all

int page_id_sort(WriteBuffer *a, WriteBuffer *b) {
    return (a->page_id - b->page_id);
}

//used in the flushing operation

int timestamp_sort(WriteBuffer *a, WriteBuffer *b) {
    return (a->timestamp_ms - b->timestamp_ms);
}

//used to sort the pages to compose flushing units

int nodes_to_flush_comp(const void * elem1, const void * elem2) {
    ChosenPage *f = (ChosenPage*) elem1;
    ChosenPage *s = (ChosenPage*) elem2;
    return (f->page_id - s->page_id);
}

static void modified_area(WriteBuffer *entry, uint8_t index_type, double *area) {
    BBox *un = NULL;
    //we have to calculate the modified area
    if (index_type == eFIND_RTREE_TYPE ||
            index_type == eFIND_RSTARTREE_TYPE) {
        REntry* re;
        if (entry->status == eFIND_STATUS_NEW || entry->status == eFIND_STATUS_MOD) {
            eFIND_RB_Node *rbnode;
            int i = 0;

            /*lets iterating our tree in order to create the first_list*/
            for (rbnode = rb_first(&entry->rb_tree); rbnode; rbnode = rb_next(rbnode)) {
                re = (REntry*) container_of(rbnode, eFIND_Modification, node)->entry;
                if (re->bbox != NULL) {
                    if (i == 0) {
                        un = bbox_clone(re->bbox);
                    } else {
                        bbox_increment_union(re->bbox, un);
                    }
                    i++;
                }
            }
            //the modified area
            if (un != NULL) {
                *area = bbox_area(un);
                lwfree(un);
            }
        } else {
            *area = 1.0;
        }
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
        if (entry->node_height == 0) {
            REntry* re;
            if (entry->status == eFIND_STATUS_NEW || entry->status == eFIND_STATUS_MOD) {
                eFIND_RB_Node *rbnode;
                int i = 0;

                /*lets iterating our tree in order to create the first_list*/
                for (rbnode = rb_first(&entry->rb_tree); rbnode; rbnode = rb_next(rbnode)) {
                    re = (REntry*) container_of(rbnode, eFIND_Modification, node)->entry;
                    if (re->bbox != NULL) {
                        if (i == 0) {
                            un = bbox_clone(re->bbox);
                        } else {
                            bbox_increment_union(re->bbox, un);
                        }
                        i++;
                    }
                }
                //the modified area
                if (un != NULL) {
                    *area = bbox_area(un);
                    lwfree(un);
                }
            } else {
                *area = 1.0;
            }
        } else {
            HilbertIEntry* re;
            if (entry->status == eFIND_STATUS_NEW || entry->status == eFIND_STATUS_MOD) {
                eFIND_RB_Node *rbnode;
                int i = 0;

                /*lets iterating our tree in order to create the first_list*/
                for (rbnode = rb_first(&entry->rb_tree); rbnode; rbnode = rb_next(rbnode)) {
                    re = (HilbertIEntry*) container_of(rbnode, eFIND_Modification, node)->entry;
                    if (re->bbox != NULL) {
                        if (i == 0) {
                            un = bbox_clone(re->bbox);
                        } else {
                            bbox_increment_union(re->bbox, un);
                        }
                        i++;
                    }
                }
                //the modified area
                if (un != NULL) {
                    *area = bbox_area(un);
                    lwfree(un);
                }
            } else {
                *area = 1.0;
            }
        }
    } else {
        *area = 1.0;
    }
}

static void modified_overlapped_area(WriteBuffer *entry, uint8_t index_type,
        double *area, double *ov_area) {
    BBox *un = NULL;

    //we have to calculate the modified area and its maximum modified area
    //we have also to calculate the overlapping area of the modified area
    if (index_type == eFIND_RTREE_TYPE ||
            index_type == eFIND_RSTARTREE_TYPE) {
        REntry **valid_entries;
        //the quantity of valid entries and aux
        int n_valid_entries = 0;
        REntry *re;

        valid_entries = (REntry**) lwalloc(sizeof (REntry*) * (n_valid_entries + 1));

        if (entry->status == eFIND_STATUS_NEW || entry->status == eFIND_STATUS_MOD) {
            eFIND_RB_Node *rbnode;

            /*lets iterating our tree in order to create the first_list*/
            for (rbnode = rb_first(&entry->rb_tree); rbnode; rbnode = rb_next(rbnode)) {
                re = (REntry*) container_of(rbnode, eFIND_Modification, node)->entry;
                if (re->bbox != NULL) {
                    if (n_valid_entries == 0) {
                        un = bbox_clone(re->bbox);
                    } else {
                        bbox_increment_union(re->bbox, un);
                    }

                    valid_entries[n_valid_entries] = re;
                    n_valid_entries++;
                    valid_entries = (REntry**) lwrealloc(valid_entries,
                            sizeof (REntry*) * (n_valid_entries + 1));
                }
            }
            //the modified area
            if (un != NULL) {
                *area = bbox_area(un);
                lwfree(un);
            }

            //the overlapping modified area
            if (n_valid_entries > 0) {
                *ov_area = rentries_overlapping_area((const REntry**) valid_entries, n_valid_entries);
            }

            //here we only free the array of entries and not their contained objects
            lwfree(valid_entries);
        } else {
            *area = 1.0;
            *ov_area = 1.0;
        }
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
        if (entry->node_height == 0) {
            REntry **valid_entries;
            //the quantity of valid entries and aux
            int n_valid_entries = 0;
            REntry *re;

            valid_entries = (REntry**) lwalloc(sizeof (REntry*) * (n_valid_entries + 1));

            if (entry->status == eFIND_STATUS_NEW || entry->status == eFIND_STATUS_MOD) {
                eFIND_RB_Node *rbnode;

                /*lets iterating our tree in order to create the first_list*/
                for (rbnode = rb_first(&entry->rb_tree); rbnode; rbnode = rb_next(rbnode)) {
                    re = (REntry*) container_of(rbnode, eFIND_Modification, node)->entry;
                    if (re->bbox != NULL) {
                        if (n_valid_entries == 0) {
                            un = bbox_clone(re->bbox);
                        } else {
                            bbox_increment_union(re->bbox, un);
                        }

                        valid_entries[n_valid_entries] = re;
                        n_valid_entries++;
                        valid_entries = (REntry**) lwrealloc(valid_entries,
                                sizeof (REntry*) * (n_valid_entries + 1));
                    }
                }
                //the modified area
                if (un != NULL) {
                    *area = bbox_area(un);
                    lwfree(un);
                }

                //the overlapping modified area
                if (n_valid_entries > 0) {
                    *ov_area = rentries_overlapping_area((const REntry**) valid_entries, n_valid_entries);
                }

                //here we only free the array of entries and not their contained objects
                lwfree(valid_entries);
            } else {
                *area = 1.0;
                *ov_area = 1.0;
            }
        } else {
            HilbertIEntry **valid_entries;
            //the quantity of valid entries and aux
            int n_valid_entries = 0;
            HilbertIEntry *re;

            valid_entries = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * (n_valid_entries + 1));

            if (entry->status == eFIND_STATUS_NEW || entry->status == eFIND_STATUS_MOD) {
                eFIND_RB_Node *rbnode;

                /*lets iterating our tree in order to create the first_list*/
                for (rbnode = rb_first(&entry->rb_tree); rbnode; rbnode = rb_next(rbnode)) {
                    re = (HilbertIEntry*) container_of(rbnode, eFIND_Modification, node)->entry;
                    if (re->bbox != NULL) {
                        if (n_valid_entries == 0) {
                            un = bbox_clone(re->bbox);
                        } else {
                            bbox_increment_union(re->bbox, un);
                        }

                        valid_entries[n_valid_entries] = re;
                        n_valid_entries++;
                        valid_entries = (HilbertIEntry**) lwrealloc(valid_entries,
                                sizeof (HilbertIEntry*) * (n_valid_entries + 1));
                    }
                }
                //the modified area
                if (un != NULL) {
                    *area = bbox_area(un);
                    lwfree(un);
                }

                //the overlapping modified area
                if (n_valid_entries > 0) {
                    *ov_area = hilbertientries_overlapping_area((const HilbertIEntry**) valid_entries, n_valid_entries);
                }

                //here we only free the array of entries and not their contained objects
                lwfree(valid_entries);
            } else {
                *area = 1.0;
                *ov_area = 1.0;
            }
        }
    } else {
        *area = 1.0;
        *ov_area = 1.0;
    }
}

static void max_modified_area(uint8_t index_type, double *max_area) {
    WriteBuffer *entry;
    double a;
    *max_area = 1.0;

    for (entry = wb; entry != NULL; entry = (WriteBuffer*) (entry->hh.next)) {
        modified_area(entry, index_type, &a);
        if (*max_area < a) {
            *max_area = a;
        }
    }
}

static void max_modified_overlapped_area(uint8_t index_type,
        double *max_area, double *max_ov_area) {
    WriteBuffer *entry;
    double a, oa;
    *max_area = 1.0;
    *max_ov_area = 1.0;

    for (entry = wb; entry != NULL; entry = (WriteBuffer*) (entry->hh.next)) {
        modified_overlapped_area(entry, index_type, &a, &oa);
        if (*max_area < a) {
            *max_area = a;
        }
        if (*max_ov_area < oa) {
            *max_ov_area = oa;
        }
    }
}

void efind_flushing(const SpatialIndex *base, eFINDSpecification * spec) {
    uint8_t index_type;
    double maxV;
    int i, f, aux = 0; //counters and auxiliary variables
    int numberOfPages; //number of considered nodes
    int numberOfFU; //number of flushing units
    double max_a = 1.0, max_oa = 1.0; //maximum modified area and maximum modified overlapping area
    unsigned mod_total = HASH_COUNT(wb); //total of modified pages contained in the write buffer

    //an array of pages picked from the first step of the flushing algorithm
    ChosenPage *chosenPages;
    //an array of pages to form the flushing units
    ChosenPage *pagesToFU;
    //the flushing units
    eFINDFlushingUnit *fus;
    WriteBuffer *s;
    //the chosen flushing unit to be written, which is an index of fus
    int chosen_fu = 0;

    uint8_t *buf, *loc;
    size_t buf_size;
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();

    _flushing_num++;
#endif

    index_type = spatialindex_get_type(base);

    /* first step: get a number of modified pages stored in the write buffer */
    if (spec->flushing_policy != eFIND_M_FP) {
        /* if the flushing policy considers the time: sort all the modified nodes by the timestamp*/
        HASH_SORT(wb, timestamp_sort);

        /* then get a percentage of the modified nodes*/
        numberOfPages = (int) (mod_total * (spec->timestamp_perc / 100.0));

        //_DEBUGF(NOTICE, "The number of modified nodes considered is %d"
        //        " and the total of modified nodes is %d", numberOfPages, mod_total);

        //if n is lesser than the flushing unit size, n will be equal to the flushing unit size
        if (numberOfPages < spec->flushing_unit_size) {
            //if the quantity of modifications contained in the buffer is lesser than the flushing unit size, n will be equal to the mod_total
            if (mod_total < spec->flushing_unit_size)
                numberOfPages = mod_total;
            else
                numberOfPages = spec->flushing_unit_size;
        }

        //_DEBUGF(NOTICE, "effectively, we will consider %d modifications", numberOfPages);
    } else {
        /* otherwise, we are considering only the quantity of modifications as flushing policy*/
        numberOfPages = mod_total;
    }

    //_DEBUG(NOTICE, "first step processed");

    /*getting the area and overlapping area only if the flushing policy requires it*/
    if (spec->flushing_policy == eFIND_MTHA_FP) {
        max_modified_area(index_type, &max_a);
    } else if (spec->flushing_policy == eFIND_MTHAO_FP) {
        max_modified_overlapped_area(index_type, &max_a, &max_oa);
    }

    /* ok, we now have the number of pages to be considered, let us get them
     * we put them into chosenPages array */
    i = 0;
    chosenPages = (ChosenPage*) lwalloc(numberOfPages * sizeof (ChosenPage));
    for (s = wb; s != NULL; s = (WriteBuffer*) (s->hh.next)) {
        chosenPages[i].height = s->node_height;
        chosenPages[i].page_id = s->page_id;
        chosenPages[i].nofmod = s->modify_count;

        /*we calculate some values to be used in the third step (see below)*/

        /*getting the area and overlapping area only if the flushing policy requires it*/
        if (spec->flushing_policy == eFIND_MTHA_FP) {
            modified_area(s, index_type, &(chosenPages[i].area));
            chosenPages[i].area = DB_MIN(1.0, (chosenPages[i].area / max_a));
        } else if (spec->flushing_policy == eFIND_MTHAO_FP) {
            modified_overlapped_area(s, index_type, &(chosenPages[i].area), &(chosenPages[i].ov_area));
            chosenPages[i].area = DB_MIN(1.0, (chosenPages[i].area / max_a));
            chosenPages[i].ov_area = DB_MIN(1.0, (chosenPages[i].ov_area / max_oa));
        }
        //_DEBUGF(NOTICE, "timestamp = %d, mc = %d, node = %d", s->timestamp_ms, chosenPages[i].nofmod, chosenPages[i].page_id);

        i++;
        if (i == numberOfPages)
            break;
    }

    /* second step: filter the chosen pages if a temporal control for writes is been used*/
    pagesToFU = efind_temporal_control_for_writes(spec, chosenPages, numberOfPages, &aux);
    if (pagesToFU == NULL) {
        //this means that there is not a temporal control support for writes
        pagesToFU = chosenPages;
    } else {
        //we free the last version
        lwfree(chosenPages);
        chosenPages = pagesToFU;
        //and update the number of pages
        numberOfPages = aux;
    }
    //_DEBUG(NOTICE, "Second step processed");

    /* third step: considering the selected pages, sort them by its identifiers
     * and construct all the possible flushing units. 
     * Each flushing unit has a value v, which is considered later 
     and calculated considering a flushing policy*/
    qsort(chosenPages, numberOfPages, sizeof (ChosenPage), nodes_to_flush_comp);
    numberOfFU = (numberOfPages + spec->flushing_unit_size - 1) / spec->flushing_unit_size;
    fus = (eFINDFlushingUnit*) lwalloc(numberOfFU * sizeof (eFINDFlushingUnit));
    f = 0;
    fus[f].n = 0;
    fus[f].v = 0.0;
    fus[f].heights = (int*) lwalloc(spec->flushing_unit_size * sizeof (int));
    fus[f].pages = (int*) lwalloc(spec->flushing_unit_size * sizeof (int));
    for (i = 0; i < numberOfPages; i++) {
        if (fus[f].n == spec->flushing_unit_size) {
            f++;
            fus[f].n = 0;
            fus[f].v = 0.0;
            fus[f].heights = (int*) lwalloc(spec->flushing_unit_size * sizeof (int));
            fus[f].pages = (int*) lwalloc(spec->flushing_unit_size * sizeof (int));
        }
        fus[f].pages[fus[f].n] = chosenPages[i].page_id;
        fus[f].heights[fus[f].n] = chosenPages[i].height;
        if (spec->flushing_policy == eFIND_MT_FP) {
            fus[f].v += chosenPages[i].nofmod;
        } else if (spec->flushing_policy == eFIND_MTH_FP) {
            fus[f].v += (chosenPages[i].nofmod * (chosenPages[i].height + 1));
        } else if (spec->flushing_policy == eFIND_MTHA_FP) {
            fus[f].v += (chosenPages[i].nofmod * (chosenPages[i].height + 1) * chosenPages[i].area);
        } else if (spec->flushing_policy == eFIND_MTHAO_FP) {
            fus[f].v += (chosenPages[i].nofmod * (chosenPages[i].height + 1) * chosenPages[i].area * chosenPages[i].ov_area);
        }

        fus[f].n++;
        //_DEBUGF(NOTICE, "fu=%d, mc = %d, node = %d, h = %d, a = %f, o = %f", f, chosenPages[i].nofmod, chosenPages[i].page_id, chosenPages[i].height, chosenPages[i].area, chosenPages[i].ov_area);
    }

    f++; //we increment f because it should correspond to the number of flushing units

    if (f != numberOfFU) {
        _DEBUGF(ERROR, "The number of flushing units do not match! (%d, %d)",
                f, numberOfFU);
    }

    //_DEBUG(NOTICE, "third step processed");

    /* fourth step: pick and write the flushing unit with the highest v*/
    maxV = 0;
    for (i = 0; i < numberOfFU; i++) {
        if (DB_GT(fus[i].v, maxV)) {
            maxV = fus[i].v;
            chosen_fu = i;
        }
    }

    //_DEBUG(NOTICE, "the flushing unit chosen is composed by: ")
    //for (i = 0; i < fus[chosen_fu].n; i++) {
    //    _DEBUGF(NOTICE, "node %d", fus[chosen_fu].pages[i]);
    //}

    buf_size = fus[chosen_fu].n * base->gp->page_size;

    if (base->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, base->gp->page_size, buf_size)) {
            _DEBUG(ERROR, "Allocation failed at execute_flushing");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(buf_size);
    }

    //_DEBUG(NOTICE, "fourth step processed");

    loc = buf;

    if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
        UIPage *p;
        RNode *node;
        //we serialize all the selected nodes
        for (i = 0; i < fus[chosen_fu].n; i++) {
            node = (RNode*) efind_buf_retrieve_node(base, spec,
                    fus[chosen_fu].pages[i], fus[chosen_fu].heights[i]);

            if (efind_temporal_control_for_reads(base, spec,
                    fus[chosen_fu].pages[i], fus[chosen_fu].heights[i], (void*) node, index_type) == NOT_INSERTED) {
                p = efind_pagehandler_create(node, index_type);
                //we check if we need to update some node in the read buffer since we will flush it
                efind_check_needed_update_in_readbuffer(base, spec,
                        fus[chosen_fu].pages[i], fus[chosen_fu].heights[i], p);
                lwfree(p);
            }
            efind_add_write_temporal_control(spec, fus[chosen_fu].pages[i]);

            rnode_serialize(node, loc);
            if (node != NULL)
                rnode_free(node);
            loc += base->gp->page_size;
        }
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
        UIPage *p;
        HilbertRNode *node;
        //we serialize all the selected nodes
        for (i = 0; i < fus[chosen_fu].n; i++) {
            node = (HilbertRNode*) efind_buf_retrieve_node(base, spec,
                    fus[chosen_fu].pages[i], fus[chosen_fu].heights[i]);

            if (efind_temporal_control_for_reads(base, spec,
                    fus[chosen_fu].pages[i], fus[chosen_fu].heights[i], (void*) node, index_type) == NOT_INSERTED) {
                p = efind_pagehandler_create(node, index_type);
                //we check if we need to update some node in the read buffer since we will flush it
                efind_check_needed_update_in_readbuffer(base, spec,
                        fus[chosen_fu].pages[i], fus[chosen_fu].heights[i], p);
                lwfree(p);
            }
            efind_add_write_temporal_control(spec, fus[chosen_fu].pages[i]);

            hilbertnode_serialize(node, loc);
            if (node != NULL)
                hilbertnode_free(node);
            loc += base->gp->page_size;
        }
    } else {
        _DEBUGF(ERROR, "eFIND does not support this index (%d) yet.", index_type);
    }

    //_DEBUG(NOTICE, "writing in batch");

    //we write in batch
    storage_write_pages(base, fus[chosen_fu].pages, buf, fus[chosen_fu].heights, fus[chosen_fu].n);

#ifdef COLLECT_STATISTICAL_DATA
    _flushed_nodes_num += fus[chosen_fu].n;
#endif

    //_DEBUG(NOTICE, "Registering in the log");

    efind_write_log_flush(base, spec, fus[chosen_fu].pages, fus[chosen_fu].n);

    //_DEBUG(NOTICE, "Done");

    //_DEBUG(NOTICE, "Cleaning memory");

    //we remove them from the buffer
    for (i = 0; i < fus[chosen_fu].n; i++) {
        efind_free_hashvalue(fus[chosen_fu].pages[i], index_type);
    }
    //free used memory    
    for (i = 0; i < numberOfFU; i++) {
        lwfree(fus[i].heights);
        lwfree(fus[i].pages);
    }
    lwfree(fus);

    lwfree(chosenPages);
    if (base->gp->io_access == DIRECT_ACCESS)
        free(buf);
    else
        lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _flushing_cpu_time += get_elapsed_time(cpustart, cpuend);
    _flushing_time += get_elapsed_time(start, end);
#endif
}

void efind_flushing_all(const SpatialIndex *base, eFINDSpecification * spec) {
    unsigned total = HASH_COUNT(wb); //the number of nodes with modifications (from the writebuffer)
    int *node_pages;
    int *node_heights;
    int n, i;
    WriteBuffer *s;
    uint8_t *buf, *loc;
    size_t buf_size;
    uint8_t index_type;
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();

    _flushing_num++;
#endif

    if (total == 0) {
        return;
    }

    index_type = spatialindex_get_type(base);

    //we sort by the node_page in order to sequentially write these nodes
    HASH_SORT(wb, page_id_sort);

    /*we write in the flash all nodes of the write buffer*/
    buf_size = total * base->gp->page_size;
    node_pages = (int*) lwalloc(sizeof (int)*total);
    node_heights = (int*) lwalloc(sizeof (int)*total);

    if (base->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, base->gp->page_size, buf_size)) {
            _DEBUG(ERROR, "Allocation failed at execute_flushing");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(buf_size);
    }

    loc = buf;
    n = 0;

    if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
        RNode *node;
        UIPage *p;
        //iterating the hash after the sorting by the node_id
        for (s = wb; s != NULL; s = (WriteBuffer*) (s->hh.next)) {
            node = (RNode*) efind_buf_retrieve_node(base, spec, s->page_id, s->node_height);
            node_pages[n] = s->page_id;
            node_heights[n] = s->node_height;

            if (efind_temporal_control_for_reads(base,
                    spec, s->page_id, s->node_height, (void*) node, index_type) == NOT_INSERTED) {
                p = efind_pagehandler_create(node, index_type);
                //we check if we need to update some node in the read buffer since we will flush it
                efind_check_needed_update_in_readbuffer(base, spec, node_pages[n], node_heights[n], p);
                lwfree(p);
            }
            efind_add_write_temporal_control(spec, node_pages[n]);

            rnode_serialize(node, loc);
            if (node != NULL)
                rnode_free(node);
            loc += base->gp->page_size;

            n++;
        }
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
        HilbertRNode *node;
        UIPage *p;
        //iterating the hash after the sorting by the node_id
        for (s = wb; s != NULL; s = (WriteBuffer*) (s->hh.next)) {
            node = (HilbertRNode*) efind_buf_retrieve_node(base, spec, s->page_id, s->node_height);
            node_pages[n] = s->page_id;
            node_heights[n] = s->node_height;

            if (efind_temporal_control_for_reads(base,
                    spec, s->page_id, s->node_height, (void*) node, index_type) == NOT_INSERTED) {
                p = efind_pagehandler_create(node, index_type);
                //we check if we need to update some node in the read buffer since we will flush it
                efind_check_needed_update_in_readbuffer(base, spec, node_pages[n], node_heights[n], p);
                lwfree(p);
            }
            efind_add_write_temporal_control(spec, node_pages[n]);

            hilbertnode_serialize(node, loc);
            if (node != NULL)
                hilbertnode_free(node);
            loc += base->gp->page_size;

            n++;
        }
    } else {
        _DEBUGF(ERROR, "eFIND does not support this index (%d) yet.", index_type);
    }


    if (n != total) {
        _DEBUG(ERROR, "The number of serialized nodes does not match "
                "with the number of nodes in the buffer at efind_flushing_all");
    }
    //we write in batch
    storage_write_pages(base, node_pages, buf, node_heights, n);

    //_DEBUG(NOTICE, "Registering in the log");

    efind_write_log_flush(base, spec, node_pages, n);

#ifdef COLLECT_STATISTICAL_DATA
    _flushed_nodes_num += n;
    _cur_buffer_size -= efind_write_buffer_size;
#endif

    //we remove them from the buffer
    for (i = 0; i < n; i++) {
        efind_free_hashvalue(node_pages[i], index_type);
    }
    //our writebuffer size is now 0
    efind_write_buffer_size = 0;
    //free used memory
    lwfree(node_pages);
    lwfree(node_heights);
    if (base->gp->io_access == DIRECT_ACCESS)
        free(buf);
    else
        lwfree(buf);
#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _flushing_cpu_time += get_elapsed_time(cpustart, cpuend);
    _flushing_time += get_elapsed_time(start, end);

    //todo fix the problem with this counter (it is a negative number because of the frequency changes between created/deleted nodes)
    _cur_mod_node_buffer_num = 0;
#endif
}

void efind_write_buf_destroy(uint8_t index_type) {
    WriteBuffer *buf_entry, *temp;

    HASH_ITER(hh, wb, buf_entry, temp) {
        HASH_DEL(wb, buf_entry);
        //we will free some space from buffer here    
        if (buf_entry->status == eFIND_STATUS_MOD)
            efind_writebuffer_destroy_mods(&buf_entry->rb_tree, index_type, buf_entry->node_height);
        lwfree(buf_entry);
    }

#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size -= efind_write_buffer_size;
#endif

    //we update buffer size
    efind_write_buffer_size = 0;
}

void efind_read_buf_destroy(const eFINDSpecification *spec, uint8_t index_type) {
    if (spec->read_buffer_policy == eFIND_LRU_RBP) {
        efind_readbuffer_lru_destroy(index_type);
    } else if (spec->read_buffer_policy == eFIND_HLRU_RBP) {
        return efind_readbuffer_hlru_destroy(index_type);
    } else if (spec->read_buffer_policy == eFIND_S2Q_RBP) {
        return efind_readbuffer_s2q_destroy(index_type);
    } else if (spec->read_buffer_policy == eFIND_2Q_RBP) {
        return efind_readbuffer_2q_destroy(index_type);
    } else if (spec->read_buffer_policy != eFIND_NONE_RBP) {
        _DEBUGF(ERROR, "The policy (%d) is not valid for the read buffer.", spec->read_buffer_policy);
    }
}
