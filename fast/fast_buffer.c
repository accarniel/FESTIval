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

#include <limits.h>

#include "../libraries/uthash/uthash.h"
#include "fast_buffer.h"
#include "fast_buffer_list_mod.h"
#include "../main/log_messages.h"
#include "fast_flush_module.h"
#include "fast_log_module.h"

#include "../main/statistical_processing.h"

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)

#undef uthash_fatal
#define uthash_fatal(msg) _DEBUG(ERROR, msg)

/*our fast_buffer is a hash map - managed by the UTHASH library*/
typedef struct FASTBuffer {
    UT_hash_handle hh;

    int hash_key;
    int nofmod; //number of modifications

    /*the height of the node (this is not counted in the buffer size)*
     * we need this value for the HLRU buffer 
     the overhead introduced here is very low because we need to know the height of the node
     * and this solution is better than to (i) create a new structure to store this value
     * or (ii) store this value into the Node structure */
    int node_height;

    uint8_t status; //FAST_STATUS_{MOD, DEL, NEW}    

    union {
        void *fast_node; //this is NULL when status is: DEL or MOD (it can be any node)    
        FASTListMod *list; //this is NULL when status is: DEL or NEW    
    } value;
} FASTBuffer;

/*
 * Some considerations about the size of the buffer:
 * 1 - it only considers the size of the hash entry + size of the key (an integer)
 * 2 - it does not consider the overhead size introduced by the UT_hash_handle!
 */
static size_t fast_buffer_size = 0; //size in bytes of our buffer
static FASTBuffer *fb = NULL;

static size_t size_of_new_hash_element(void);
static size_t size_of_new_node(void *rnode, uint8_t index_type);
static size_t size_of_pointer_mod(void);
static size_t size_of_hole_mod(void);
static size_t size_of_hilbert_value_mod(void);
static size_t size_of_bbox_mod(BBox *bbox);
static size_t size_of_del_node(void);

//static size_t fb_size_buffer(void);

size_t size_of_new_hash_element() {
    //size of the hash key, status and number of modifications
    return sizeof (int) + sizeof (uint8_t) + sizeof (int);
}

size_t size_of_new_node(void *rnode, uint8_t index_type) {
    if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
        //size of the node
        return rnode_size((RNode*) rnode);
    } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
        return hilbertnode_size((HilbertRNode*) rnode);
    } else {
        return 0;
        //TO-DO
    }
}

size_t size_of_pointer_mod() {
    //size of type, position, pointer
    return sizeof (uint8_t) + sizeof (int) + sizeof (int);
}

size_t size_of_hole_mod() {
    //size of type, position
    return sizeof (uint8_t) + sizeof (int);
}

size_t size_of_hilbert_value_mod() {
    //size of type, position, hilbert value
    return sizeof (uint8_t) + sizeof (int) + sizeof (hilbert_value_t);
}

size_t size_of_bbox_mod(BBox *bbox) {
    if (bbox == NULL) {
        //size of type and position
        return sizeof (uint8_t) + sizeof (int);
    } else {
        //size of type, position, and bbox
        return sizeof (uint8_t) + sizeof (int) + (sizeof (double) * NUM_OF_DIM * 2);
    }
}

size_t size_of_del_node() {
    //there is no required size
    return 0;
}

static bool fast_processing_hole = false;

/*we put a new node/page in the buffer -> key equal to new_node_page and the value is (NEW, a pointer to the new_node)
 if we already have a key equal to new_node_page, then we modify its value to new_node*/
void fb_put_new_node(const SpatialIndex *base, FASTSpecification *spec, int new_node_page, void *new_node, int height) {
    FASTBuffer *buf_entry;
    size_t required_size = 0;
    uint8_t index_type;

    //we get the index type
    index_type = spatialindex_get_type(base);
    if (!(index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE || index_type == FAST_HILBERT_RTREE_TYPE)) {
        _DEBUGF(ERROR, "FAST was called with a non supported spatial index (%d)", index_type);
    }

    //_DEBUGF(NOTICE, "put a modification in the buffer (NEW NODE) rnode %d", new_node_page);

    //is new_node_page already in the hash?
    HASH_FIND_INT(fb, &new_node_page, buf_entry);

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = size_of_new_hash_element() + size_of_new_node(new_node, index_type);
    } else {
        if (buf_entry->status != FAST_STATUS_DEL) {
            /* this case will never happens*/
            /* since we call this function only for nodes that are not in the hash, then
        the buf_entry is always NULL*/
            _DEBUG(ERROR, "fb_put_new_rnode: This function was called for a "
                    "rnode that already exists in the buffer.");
        } else {
            required_size = size_of_new_node(new_node, index_type);
        }
    }

    //if we do not have space, we execute the flushing
    if (spec->buffer_size < (required_size + fast_buffer_size)) {
        //_DEBUGF(NOTICE, "Flushing is needed because the size of buffer will be %d and the capacity is %d", (required_size + fast_buffer_size), spec->buffer_size);
        fast_execute_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(fb, &new_node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = size_of_new_hash_element() + size_of_new_node(new_node, index_type);
        }

        //_DEBUG(NOTICE, "Flushing operation done");
    }

    /*then we put it into our buffer because we have space now =)*/
    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (FASTBuffer*) lwalloc(sizeof (FASTBuffer));
        buf_entry->hash_key = new_node_page;
        buf_entry->nofmod = 0;
        buf_entry->node_height = height;

        //_DEBUG(NOTICE, "We have put a new entire node in the buffer: ");
        //print_rnode(new_node, new_node_page);

        HASH_ADD_INT(fb, hash_key, buf_entry); //we add it 
    }
    buf_entry->status = FAST_STATUS_NEW;
    buf_entry->value.fast_node = new_node;
    buf_entry->nofmod++;

    //we increment the fast_buffer_size
    fast_buffer_size += required_size;

    /*only for debug mode
    if (fast_buffer_size != fb_size_buffer()) {
        _DEBUGF(NOTICE, "Required size is %d", required_size);
        _DEBUGF(ERROR, "The sizes do not match! Size of the buffer: %d x size of fast_buffer_size %d",
                fb_size_buffer(), fast_buffer_size);
    }
     */

    //we also put this modification in the log
    write_log_new_node(base, spec, new_node_page, new_node, height);

    //we modify its correspondingly flushing unit
    fast_set_flushing_unit(spec, new_node_page);

    //_DEBUG(NOTICE, "put done");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_new_node_buffer_num++;
    _new_node_buffer_num++;

    _cur_buffer_size = fast_buffer_size;
#endif
}

/*this function is called when a new insertion is made in a HilbertNode!
 it is needed because an entry can be inserted in any position of a HilbertNode.*/
void fb_completed_insertion() {
    fast_processing_hole = false;
}

/*this functions is to access the value of the fast_processing_hole */
bool is_processing_hole() {
    return fast_processing_hole;
}

/*this function is only called by FAST HilbertTrees because a node 
 * has to respect the order of the hilbert values of its entries*/
void fb_put_mod_hole(const SpatialIndex *base, FASTSpecification *spec, int node_page, int position, int height) {
    FASTBuffer *buf_entry;
    size_t required_size = 0;
    uint8_t index_type;

    //we get the index type
    index_type = spatialindex_get_type(base);
    if (!(index_type == FAST_HILBERT_RTREE_TYPE)) {
        _DEBUGF(ERROR, "This function should only be called by FAST Hilbert R-trees and not for this index (%d)", index_type);
    }

    //_DEBUGF(NOTICE, "put a modification in the buffer (NEW NODE) rnode %d", new_node_page);

    //is new_node_page already in the hash?
    HASH_FIND_INT(fb, &node_page, buf_entry);

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = size_of_new_hash_element() + size_of_hole_mod();
    } else {
        if (buf_entry->status == FAST_STATUS_NEW) {
            required_size = 0;
        } else if (buf_entry->status == FAST_STATUS_DEL) {
            /* this case is not allowed since a removed page must be allocated 
             * again as new node */
            _DEBUG(ERROR, "fb_put_mod_hole: it is not allowed to add a "
                    "modification in a removed node. Use fb_put_new_node instead");
        } else {
            required_size = size_of_hole_mod();
        }
    }

    //if we do not have space, we execute the flushing
    if (required_size > 0 && spec->buffer_size < (required_size + fast_buffer_size)) {
        fast_execute_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(fb, &node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = size_of_new_hash_element() + size_of_hole_mod();
        }
    }

    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (FASTBuffer*) lwalloc(sizeof (FASTBuffer));
        buf_entry->hash_key = node_page;
        buf_entry->status = FAST_STATUS_MOD;
        buf_entry->value.list = flm_init();
        buf_entry->nofmod = 0;
        buf_entry->node_height = height;

        HASH_ADD_INT(fb, hash_key, buf_entry); //we add it  
    }
    buf_entry->nofmod++;
    //if this node was created only in main memory, we modify it
    if (buf_entry->status == FAST_STATUS_NEW) {
        HilbertRNode *hilbertnode;
        hilbertnode = (HilbertRNode *) buf_entry->value.fast_node;
        //we need to check the kind of node that is stored in this buffer
        if (hilbertnode->type == HILBERT_INTERNAL_NODE) {
            hilbertnode->entries.internal = (HilbertIEntry**) lwrealloc(hilbertnode->entries.internal, (hilbertnode->nofentries + 1) * sizeof (HilbertIEntry*));
            memmove(hilbertnode->entries.internal + (position + 1), hilbertnode->entries.internal + position,
                    sizeof (hilbertnode->entries.internal[0]) * (hilbertnode->nofentries - position));
        } else {
            hilbertnode->entries.leaf = (REntry**) lwrealloc(hilbertnode->entries.leaf, (hilbertnode->nofentries + 1) * sizeof (REntry*));
            memmove(hilbertnode->entries.leaf + (position + 1), hilbertnode->entries.leaf + position,
                    sizeof (hilbertnode->entries.leaf[0]) * (hilbertnode->nofentries - position));
        }
    } else {
        FASTModItem *flm_item;
        flm_item = (FASTModItem*) lwalloc(sizeof (FASTModItem));
        flm_item->type = FAST_ITEM_TYPE_H;
        flm_item->position = position;

        //we append the new mod in the mod list
        flm_append(buf_entry->value.list, flm_item);
    }

    fast_processing_hole = true;

    //we increment the fast_buffer_size
    fast_buffer_size += required_size;

    //we also put this modification in the log  
    write_log_mod_hole(base, spec, node_page, position, height);

    //we modify its correspondingly flushing unit
    fast_set_flushing_unit(spec, node_page);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size = fast_buffer_size;
#endif
}

/*we put a new bbox In the buffer -> key equal to node_page and the value is MOD and a triple:
 * (K, position, new_bbox)
 if we already have a key equal to node_page, then we append this modification*/
void fb_put_mod_bbox(const SpatialIndex *base, FASTSpecification *spec,
        int node_page, BBox *new_bbox, int position, int height) {
    FASTBuffer *buf_entry;
    size_t required_size = 0;
    uint8_t index_type;

    //_DEBUG(NOTICE, "Putting a mod bbox");

    //we get the index type
    index_type = spatialindex_get_type(base);
    if (!(index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE || index_type == FAST_HILBERT_RTREE_TYPE))
        _DEBUGF(ERROR, "FAST was called with a non supported spatial index (%d)", index_type);

    //_DEBUGF(NOTICE, "put a modification in the buffer (BBOX) of the rnode %d at %d", rnode_page, position);

    //is rnode_page already in the hash?
    HASH_FIND_INT(fb, &node_page, buf_entry);

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = size_of_new_hash_element() + size_of_bbox_mod(new_bbox);
    } else {
        if (buf_entry->status == FAST_STATUS_NEW) {
            //we need to check the kind of node that is stored in this buffer
            if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                RNode *rnode;
                rnode = (RNode *) buf_entry->value.fast_node;
                if (position > rnode->nofentries) {
                    /* this case is not allowed since it will put "holes" in a node */
                    _DEBUGF(ERROR, "fb_put_mod_bbox: invalid position (%d) "
                            "to add or modify an element (number of elements %d)",
                            position, rnode->nofentries);
                }

                //in this case there is no required size
                if (new_bbox == NULL && position == rnode->nofentries) {
                    required_size = 0;
                } else {
                    //in this case we have to create a new entry
                    if (position == rnode->nofentries) {
                        required_size = rentry_size();
                    }

                    //in this case we have to remove a rentry
                    if (new_bbox == NULL) {
                        required_size = -rentry_size();
                    }
                }
            } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                HilbertRNode *hilbertnode;
                hilbertnode = (HilbertRNode *) buf_entry->value.fast_node;
                if (position == hilbertnode->nofentries) {
                    _DEBUG(ERROR, "We cannot create new entries using this method. "
                            "You should first call the modification for pointer.");
                }

                if (hilbertnode->type == HILBERT_INTERNAL_NODE) {
                    //in this case we have to remove an rentry
                    if (new_bbox == NULL) {
                        required_size = -hilbertientry_size();
                    }
                } else {
                    //in this case we have to remove an rentry
                    if (new_bbox == NULL) {
                        required_size = -rentry_size();
                    }
                }
            }
        } else if (buf_entry->status == FAST_STATUS_DEL) {
            /* this case is not allowed since a removed page must be allocated 
             * again as new node */
            _DEBUG(ERROR, "fb_put_mod_bbox: it is not allowed to add a "
                    "modification in a removed node. Use fb_put_new_node instead");
        } else {
            required_size = size_of_bbox_mod(new_bbox);
        }
    }

    //if we do not have space, we execute the flushing
    if (required_size > 0 && spec->buffer_size < (required_size + fast_buffer_size)
            && !fast_processing_hole) {
        //    _DEBUGF(NOTICE, "Flushing is needed because the size of buffer will be %d and the capacity is %d", (required_size + fast_buffer_size),                spec->buffer_size);
        fast_execute_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(fb, &node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = size_of_new_hash_element() + size_of_bbox_mod(new_bbox);
        }

        //    _DEBUG(NOTICE, "Flushing operation done");
    }

    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (FASTBuffer*) lwalloc(sizeof (FASTBuffer));
        buf_entry->hash_key = node_page;
        buf_entry->status = FAST_STATUS_MOD;
        buf_entry->value.list = flm_init();
        buf_entry->nofmod = 0;
        buf_entry->node_height = height;

        HASH_ADD_INT(fb, hash_key, buf_entry); //we add it  
    }
    buf_entry->nofmod++;
    //if this node was created only in main memory, we modify it
    if (buf_entry->status == FAST_STATUS_NEW) {
        //we need to check the kind of node that is stored in this buffer
        if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
            RNode *rnode;
            rnode = (RNode *) buf_entry->value.fast_node;
            //in this case we have to create a new entry
            if (position == rnode->nofentries) {
                rnode_add_rentry(rnode, rentry_create(-1, new_bbox));
            }

            //in this case we have to remove a rentry
            if (new_bbox == NULL) {
                rnode_remove_rentry(rnode, position);
            } else {
                //otherwise we have to update the entry
                memcpy(rnode->entries[position]->bbox, new_bbox, sizeof (BBox));
                //we can free it here since it always will be a copy
                lwfree(new_bbox);
            }
        } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
            HilbertRNode *hilbertnode;
            hilbertnode = (HilbertRNode *) buf_entry->value.fast_node;

            //in this case we have to remove an entry
            if (new_bbox == NULL) {
                hilbertnode_remove_entry(hilbertnode, position);
            } else {
                //otherwise we have to update an entry
                if (hilbertnode->type == HILBERT_INTERNAL_NODE) {
                    memcpy(hilbertnode->entries.internal[position]->bbox, new_bbox, sizeof (BBox));
                } else {
                    memcpy(hilbertnode->entries.leaf[position]->bbox, new_bbox, sizeof (BBox));
                }
                //we can free it here since it always will be a copy
                lwfree(new_bbox);
            }
        }
    } else {
        FASTModItem *flm_item;
        flm_item = (FASTModItem*) lwalloc(sizeof (FASTModItem));
        flm_item->type = FAST_ITEM_TYPE_K;
        flm_item->position = position;
        //when new_bbox is NULL, then we consider that entry was removed
        flm_item->value.bbox = new_bbox;

        //we append the new bbox in the mod list
        flm_append(buf_entry->value.list, flm_item);
    }

    //we increment the fast_buffer_size
    fast_buffer_size += required_size;

    /* only for debug mode 
    if (fast_buffer_size != fb_size_buffer()) {
        _DEBUGF(NOTICE, "REQUIRED SIZE %d", required_size);
        _DEBUGF(ERROR, "The sizes do not match! Size of the buffer: %d x size of fast_buffer_size %d",
                fb_size_buffer(), fast_buffer_size);
    }
     */
    //we also put this modification in the log    
    write_log_mod_bbox(base, spec, node_page, new_bbox, position, height);

    //we modify its correspondingly flushing unit
    fast_set_flushing_unit(spec, node_page);

    //_DEBUG(NOTICE, "OK FOR THE MOD BBOX");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_mod_node_buffer_num++;
    _mod_node_buffer_num++;

    _cur_buffer_size = fast_buffer_size;
#endif
}

/*we put a new pointer In the buffer -> key equal to rnode_page and the value is MOD and a triple:
 * (P, position, new_pointer)
 if we already have a key equal to rnode_page, then we append this modification
 IMPORTANT NOTE: If a pointer has to be removed from a node, please use the fb_put_new_bbox
 passing NULL for the new value of the bbox!*/
void fb_put_mod_pointer(const SpatialIndex *base, FASTSpecification *fs,
        int node_page, int new_pointer, int position, int height) {
    FASTBuffer *buf_entry;
    size_t required_size = 0;
    uint8_t index_type;

    //_DEBUG(NOTICE, "Putting a mod pointer");

    //we get the index type
    index_type = spatialindex_get_type(base);
    if (!(index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE || index_type == FAST_HILBERT_RTREE_TYPE))
        _DEBUGF(ERROR, "FAST was called with a non supported spatial index (%d)", index_type);

    // _DEBUGF(NOTICE, "put a modification in the buffer (POINTER) of the rnode %d at %d to the following value %d", rnode_page, position, new_pointer);

    //is rnode_page already in the hash?
    HASH_FIND_INT(fb, &node_page, buf_entry);

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = size_of_new_hash_element() + size_of_pointer_mod();
    } else {
        if (buf_entry->status == FAST_STATUS_NEW) {
            //we need to manage correctly the kind of node stored in this buffer
            if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                RNode *rnode;
                rnode = (RNode *) buf_entry->value.fast_node;
                if (position > rnode->nofentries) {
                    /* this case is not allowed since it will put "holes" in a node */
                    _DEBUGF(ERROR, "fb_put_mod_pointer: invalid position (%d) "
                            "to add or modify an element (number of elements %d)",
                            position, rnode->nofentries);
                }

                //in this case we have to create a new entry
                if (position == rnode->nofentries) {
                    required_size = rentry_size();
                }
                /*there is no remotion here since a deletion operation 
                is done by the function fb_put_mod_bbox*/
            } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                HilbertRNode *hilbertnode;
                hilbertnode = (HilbertRNode *) buf_entry->value.fast_node;
                if (position > hilbertnode->nofentries) {
                    /* this case is not allowed since it will put "holes" in a node */
                    _DEBUGF(ERROR, "fb_put_mod_pointer: invalid position (%d) "
                            "to add or modify an element (number of elements %d)",
                            position, hilbertnode->nofentries);
                }

                if (hilbertnode->type == HILBERT_INTERNAL_NODE) {
                    //in this case we have to create a new entry
                    if (position == hilbertnode->nofentries || fast_processing_hole) {
                        required_size = hilbertientry_size();
                    }
                } else {
                    //in this case we have to create a new entry
                    if (position == hilbertnode->nofentries || fast_processing_hole) {
                        required_size = rentry_size();
                    }
                }
                /*there is no remotion here since a deletion operation 
                is done by the function fb_put_mod_bbox*/
            }
        } else if (buf_entry->status == FAST_STATUS_DEL) {
            /* this case is not allowed since a removed page must be allocated 
             * again as new node */
            _DEBUG(ERROR, "fb_put_mod_pointer: it is not allowed to add a "
                    "modification in a removed node. Use fb_put_new_rnode instead");
        } else {
            required_size = size_of_pointer_mod();
        }
    }

    //if we do not have space, we execute the flushing
    if (required_size > 0 && fs->buffer_size < (required_size + fast_buffer_size)
            && !fast_processing_hole) {
        //_DEBUGF(NOTICE, "Flushing is needed because the size of buffer will be %d and the capacity is %d", (required_size + fast_buffer_size),                fs->buffer_size);
        fast_execute_flushing(base, fs);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(fb, &node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = size_of_new_hash_element() + size_of_pointer_mod();
        }

        //_DEBUG(NOTICE, "Flushing operation done");
    }

    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (FASTBuffer*) lwalloc(sizeof (FASTBuffer));
        buf_entry->hash_key = node_page;
        buf_entry->status = FAST_STATUS_MOD;
        buf_entry->value.list = flm_init();
        buf_entry->nofmod = 0;
        buf_entry->node_height = height;
        HASH_ADD_INT(fb, hash_key, buf_entry); //we add it        
    }
    buf_entry->nofmod++;
    //if this node was created only in main memory, we modify it
    if (buf_entry->status == FAST_STATUS_NEW) {
        //we need to manage correctly the kind of node stored in this buffer
        if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
            RNode *rnode;
            rnode = (RNode *) buf_entry->value.fast_node;
            //in this case (the unique possible case), we need to create a new entry
            if (position == rnode->nofentries) {
                rnode_add_rentry(rnode, rentry_create(-1, bbox_create()));
            }
            rnode->entries[position]->pointer = new_pointer;
        } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
            HilbertRNode *hilbertnode;
            hilbertnode = (HilbertRNode *) buf_entry->value.fast_node;
            if (hilbertnode->type == HILBERT_INTERNAL_NODE) {
                if (hilbertnode->entries.internal == NULL) {
                    hilbertnode->entries.internal = (HilbertIEntry**) lwalloc(1 * sizeof (HilbertIEntry*));
                } else if (position == hilbertnode->nofentries) {
                    //we only create more space, if we are adding an entry in the last position
                    //otherwise, space was already given by the fb_put_hole
                    hilbertnode->entries.internal = (HilbertIEntry**) lwrealloc(hilbertnode->entries.internal,
                            (hilbertnode->nofentries + 1) * sizeof (HilbertIEntry*));
                }
                //in this case we have to create a new entry
                hilbertnode->entries.internal[position] = (HilbertIEntry*) lwalloc(sizeof (HilbertIEntry));
                hilbertnode->entries.internal[position]->bbox = bbox_create();
                hilbertnode->entries.internal[position]->pointer = new_pointer;
            } else {
                if (hilbertnode->entries.leaf == NULL) {
                    hilbertnode->entries.leaf = (REntry**) lwalloc(1 * sizeof (REntry*));
                } else if (position == hilbertnode->nofentries) {
                    //we only create more space, if we are adding an entry in the last position
                    //otherwise, space was already given by the fb_put_hole
                    hilbertnode->entries.leaf = (REntry**) lwrealloc(hilbertnode->entries.leaf,
                            (hilbertnode->nofentries + 1) * sizeof (REntry*));
                }
                //in this case we have to create a new entry
                hilbertnode->entries.leaf[position] = (REntry*) lwalloc(sizeof (REntry));
                hilbertnode->entries.leaf[position]->bbox = bbox_create();
                hilbertnode->entries.leaf[position]->pointer = new_pointer;
            }
            //in this case we have created a new entry
            if(hilbertnode->nofentries == position || fast_processing_hole) {
                hilbertnode->nofentries++;
            }
        }
    } else {
        FASTModItem *flm_item;
        flm_item = (FASTModItem*) lwalloc(sizeof (FASTModItem));
        flm_item->type = FAST_ITEM_TYPE_P;
        flm_item->position = position;
        flm_item->value.pointer = new_pointer;

        //we append the new pointer in the mod list
        flm_append(buf_entry->value.list, flm_item);
    }

    //we increment the fast_buffer_size
    fast_buffer_size += required_size;

    /* only for debug mode
    if (fast_buffer_size != fb_size_buffer()) {
        _DEBUGF(ERROR, "The sizes do not match! Size of the buffer: %d x size of fast_buffer_size %d",
                fb_size_buffer(), fast_buffer_size);
    }
     */

    //we also put this modification in the log
    write_log_mod_pointer(base, fs, node_page, new_pointer, position, height);

    //we modify its correspondingly flushing unit
    fast_set_flushing_unit(fs, node_page);

    // _DEBUG(NOTICE, "Put a pointer mod done");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_mod_node_buffer_num++;
    _mod_node_buffer_num++;

    _cur_buffer_size = fast_buffer_size;
#endif
}

/*we put a new lhv In the buffer -> key equal to rnode_page and the value is MOD and a triple:
 * (L, position, new_lhv)
 if we already have a key equal to rnode_page, then we append this modification
 IMPORTANT NOTE: If a pointer has to be removed from a node, please use the fb_put_new_bbox
 passing NULL for the new value of the bbox!
 IMPORTANTE NOTE 2: this function is only valid for Hilbert R-trees*/
void fb_put_mod_lhv(const SpatialIndex *base, FASTSpecification *fs,
        int node_page, hilbert_value_t new_lhv, int position, int height) {
    FASTBuffer *buf_entry;
    size_t required_size = 0;
    uint8_t index_type;

    //_DEBUG(NOTICE, "Putting a mod lhv");

    //we get the index type
    index_type = spatialindex_get_type(base);
    if (!(index_type == FAST_HILBERT_RTREE_TYPE))
        _DEBUGF(ERROR, "This functions should be only called for Hilbert R-trees (%d)", index_type);

    // _DEBUGF(NOTICE, "put a modification in the buffer (POINTER) of the rnode %d at %d to the following value %d", rnode_page, position, new_pointer);

    //is rnode_page already in the hash?
    HASH_FIND_INT(fb, &node_page, buf_entry);

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = size_of_new_hash_element() + size_of_hilbert_value_mod();
    } else {
        if (buf_entry->status == FAST_STATUS_NEW) {
            //we need to manage correctly the kind of node stored in this buffer
            if (index_type == FAST_HILBERT_RTREE_TYPE) {
                HilbertRNode *hilbertnode;
                hilbertnode = (HilbertRNode *) buf_entry->value.fast_node;
                if (position >= hilbertnode->nofentries) {
                    /* this case is not allowed since it will put "holes" in a node */
                    _DEBUGF(ERROR, "fb_put_mod_lhv: invalid position (%d) "
                            "to modify an element (number of elements %d)",
                            position, hilbertnode->nofentries);
                }
                /*there is no remotion here since a deletion operation 
                is done by the function fb_put_mod_bbox*/
            }
        } else if (buf_entry->status == FAST_STATUS_DEL) {
            /* this case is not allowed since a removed page must be allocated 
             * again as new node */
            _DEBUG(ERROR, "fb_put_mod_lhv: it is not allowed to add a "
                    "modification in a removed node. Use fb_put_new_rnode instead");
        } else {
            required_size = size_of_pointer_mod();
        }
    }

    //if we do not have space, we execute the flushing
    if (required_size > 0 && fs->buffer_size < (required_size + fast_buffer_size)
            && !fast_processing_hole) {
        //_DEBUGF(NOTICE, "Flushing is need because the size of buffer will be %d and the capacity is %d", (required_size + fast_buffer_size),                fs->buffer_size);
        fast_execute_flushing(base, fs);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(fb, &node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = size_of_new_hash_element() + size_of_pointer_mod();
        }

        //_DEBUG(NOTICE, "Flushing operation done");
    }

    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (FASTBuffer*) lwalloc(sizeof (FASTBuffer));
        buf_entry->hash_key = node_page;
        buf_entry->status = FAST_STATUS_MOD;
        buf_entry->value.list = flm_init();
        buf_entry->nofmod = 0;
        buf_entry->node_height = height;
        HASH_ADD_INT(fb, hash_key, buf_entry); //we add it 
    }
    buf_entry->nofmod++;
    //if this node was created only in main memory, we modify it
    if (buf_entry->status == FAST_STATUS_NEW) {
        //we need to manage correctly the kind of node stored in this buffer
        if (index_type == FAST_HILBERT_RTREE_TYPE) {
            HilbertRNode *hilbertnode;
            hilbertnode = (HilbertRNode *) buf_entry->value.fast_node;
            if (hilbertnode->type == HILBERT_INTERNAL_NODE) {
                hilbertnode->entries.internal[position]->lhv = new_lhv;
            }
        }
    } else {
        FASTModItem *flm_item;
        flm_item = (FASTModItem*) lwalloc(sizeof (FASTModItem));
        flm_item->type = FAST_ITEM_TYPE_L;
        flm_item->position = position;
        flm_item->value.lhv = new_lhv;

        //we append the new pointer in the mod list
        flm_append(buf_entry->value.list, flm_item);
    }

    //we increment the fast_buffer_size
    fast_buffer_size += required_size;

    /* only for debug mode
    if (fast_buffer_size != fb_size_buffer()) {
        _DEBUGF(ERROR, "The sizes do not match! Size of the buffer: %d x size of fast_buffer_size %d",
                fb_size_buffer(), fast_buffer_size);
    }
     */

    //we also put this modification in the log
    write_log_mod_lhv(base, fs, node_page, new_lhv, position, height);

    //we modify its correspondingly flushing unit
    fast_set_flushing_unit(fs, node_page);

    //_DEBUG(NOTICE, "Put a pointer lhv done");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_mod_node_buffer_num++;
    _mod_node_buffer_num++;

    _cur_buffer_size = fast_buffer_size;
#endif
}

/*we put a NULL pointer in the buffer -> key equal to rnode_page and the value is DEL and a value NULL*/
void fb_del_node(const SpatialIndex *base, FASTSpecification *spec, int node_page, int height) {
    FASTBuffer *buf_entry;
    long int required_size = 0;
    uint8_t index_type;

    //_DEBUG(NOTICE, "deleting a node");

    //we get the index type
    index_type = spatialindex_get_type(base);
    if (!(index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE || index_type == FAST_HILBERT_RTREE_TYPE))
        _DEBUGF(ERROR, "FAST was called with a non supported spatial index (%d)", index_type);

    //is rnode_page already in the hash?
    HASH_FIND_INT(fb, &node_page, buf_entry);

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = size_of_new_hash_element() + size_of_del_node();
    } else {
        required_size = size_of_del_node();
    }

    //if we do not have space, we execute the flushing
    if (spec->buffer_size > 0 && spec->buffer_size < (required_size + fast_buffer_size)) {
        //_DEBUG(NOTICE, "executing a flushing operation");

        fast_execute_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(fb, &node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = size_of_new_hash_element() + size_of_del_node();
        }

        //_DEBUG(NOTICE, "flushing operation done");
    }

    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (FASTBuffer*) lwalloc(sizeof (FASTBuffer));
        buf_entry->hash_key = node_page;
        buf_entry->status = FAST_STATUS_DEL;
        buf_entry->nofmod = 0;
        buf_entry->node_height = height;
        HASH_ADD_INT(fb, hash_key, buf_entry); //we add it        
    } else {
        int removed_size = 0;
        //we will free some space from buffer here
        if (buf_entry->status == FAST_STATUS_MOD) {
            FASTListMod *mods;
            FASTListItem *fli;
            FASTModItem *item;
            mods = buf_entry->value.list;
            fli = mods->first;
            while (fli != NULL) {
                item = fli->item;
                if (item->type == FAST_ITEM_TYPE_K) {
                    removed_size += size_of_bbox_mod(item->value.bbox);
                } else if (item->type == FAST_ITEM_TYPE_P) {
                    removed_size += size_of_pointer_mod();
                } else if (item->type == FAST_ITEM_TYPE_L) {
                    removed_size += size_of_hilbert_value_mod();
                } else {
                    removed_size += size_of_hole_mod();
                }
                //we move to the next modification
                fli = fli->next;
            }

            flm_destroy(buf_entry->value.list);
            buf_entry->value.list = NULL;
        } else if (buf_entry->status == FAST_STATUS_NEW) {
            removed_size += size_of_new_node(buf_entry->value.fast_node, index_type);
            //we need to free the node stored in this buffer
            if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                RNode *rnode;
                rnode = (RNode *) buf_entry->value.fast_node;
                rnode_free(rnode);
            } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                HilbertRNode *hilbertnode;
                hilbertnode = (HilbertRNode *) buf_entry->value.fast_node;
                hilbertnode_free(hilbertnode);
            }

            buf_entry->value.fast_node = NULL;
        }
        required_size = (required_size - removed_size);
        buf_entry->status = FAST_STATUS_DEL;
    }
    buf_entry->nofmod++;

    //we update (increment or decrement) the fast_buffer_size
    fast_buffer_size += required_size;

    /* only for debug mode
    if (fast_buffer_size != fb_size_buffer()) {
        _DEBUGF(ERROR, "The sizes do not match! Size of the buffer: %d x size of fast_buffer_size %d",
                fb_size_buffer(), fast_buffer_size);
    }
     */

    //we also put this modification in the log
    write_log_del_node(base, spec, node_page, height);

    //we modify its correspondingly flushing unit
    fast_set_flushing_unit(spec, node_page);

    //_DEBUG(NOTICE, "deletion done");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_del_node_buffer_num++;
    _del_node_buffer_num++;

    _cur_buffer_size = fast_buffer_size;
#endif
}

/*we retrieve the most recent version of a RNODE by considering possible modification in the buffer
 after the call, we return the most recent version of the request node (rnode_page)*/
void *fb_retrieve_node(const SpatialIndex *base, int node_page, int height) {
    FASTBuffer *buf_entry;
    void *ret = NULL;

    uint8_t index_type;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif   

    //we get the index type
    index_type = spatialindex_get_type(base);
    if (!(index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE || index_type == FAST_HILBERT_RTREE_TYPE))
        _DEBUGF(ERROR, "FAST was called with a non supported spatial index (%d)", index_type);

    //_DEBUGF(NOTICE, "Retrieving the node %d from the buffer", node_page);

    //is rnode_page already in the hash?
    HASH_FIND_INT(fb, &node_page, buf_entry);
    if (buf_entry == NULL) {
        //this node is not in the buffer
        //therefore, we have to return the node stored in the secondary memory
        if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
            ret = get_rnode(base, node_page, height);
        } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
            ret = get_hilbertnode(base, node_page, height);
        }
    } else {
        //if this node is in the buffer, we check if it is a newly created node
        if (buf_entry->status == FAST_STATUS_NEW) {
            if (buf_entry->value.fast_node == NULL) {
                _DEBUGF(ERROR, "fb_retrieve_rnode: "
                        "Node %d in the buffer is NULL", node_page);
            }

            if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                //_DEBUG(NOTICE, "It is a new node");
                //rnode_print((RNode *) buf_entry->value.fast_node, node_page);

                //we always return a copy
                ret = (void *) rnode_clone((RNode *) buf_entry->value.fast_node);
            } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                //_DEBUG(NOTICE, "It is a new node");
                ret = (void *) hilbertnode_clone((HilbertRNode*) buf_entry->value.fast_node);
            }

            //_DEBUG(NOTICE, "returned the node from the buffer");
        } else if (buf_entry->status == FAST_STATUS_MOD) {
            //if it is a modified node, we need to apply the modification list
            FASTListMod *mods;
            FASTModItem *item;
            FASTListItem *fli;

            //first, we have to read this node from the disk
            if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                ret = (void *) get_rnode(base, node_page, height);
            } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                ret = (void *) get_hilbertnode(base, node_page, height);
            }

            if (ret == NULL) {
                _DEBUGF(ERROR, "fb_retrieve_rnode: "
                        "Node %d stored in the disk is NULL", node_page);
            }

            mods = buf_entry->value.list;
            if (mods == NULL || mods->first == NULL || mods->size == 0) {
                _DEBUG(ERROR, "It should has a modification list, "
                        "but it is currently empty. "
                        "something wrong in the buffer");
                return NULL;
            }
            fli = mods->first;
            if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                RNode *rnode = (RNode *) ret;
                //we apply each modification in the modification list
                while (fli != NULL) {
                    item = fli->item;

                    if (item->position > rnode->nofentries) {
                        _DEBUGF(ERROR, "The list of modification has "
                                "a position (%d) that will introduce holes: number of"
                                " elements %d", item->position, rnode->nofentries)
                    }

                    //in this case, we have to create a new entry
                    if (item->position == rnode->nofentries) {
                        rnode_add_rentry(rnode, rentry_create(-1, bbox_create()));
                    }

                    //here, we check if this modification refers to the pointer
                    if (item->type == FAST_ITEM_TYPE_P) {
                        rnode->entries[item->position]->pointer = item->value.pointer;
                    } else {
                        //otherwise, we have to modify the BBOX of an entry
                        if (item->value.bbox == NULL) {
                            //    _DEBUGF(NOTICE, "We have to remove the following entry: %d ", item->position);
                            rnode_remove_rentry(rnode, item->position);
                        } else {
                            //    _DEBUG(NOTICE, "We have modified the bbox of this entry");
                            memcpy(rnode->entries[item->position]->bbox, item->value.bbox, sizeof (BBox));
                        }
                    }

                    //we move to the next modification
                    fli = fli->next;
                }
            } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                HilbertRNode *hilbertnode = (HilbertRNode *) ret;
                if (hilbertnode->type == HILBERT_INTERNAL_NODE) {
                    //we apply each modification in the modification list
                    while (fli != NULL) {
                        item = fli->item;

                        if (item->position > hilbertnode->nofentries) {
                            _DEBUGF(ERROR, "The list of modification has "
                                    "a position (%d) that will introduce holes: number of"
                                    " elements %d", item->position, hilbertnode->nofentries)
                        }

                        //here, we check if this modification refers to the pointer
                        if (item->type == FAST_ITEM_TYPE_H) {
                            hilbertnode->entries.internal = (HilbertIEntry**) lwrealloc(hilbertnode->entries.internal,
                                    (hilbertnode->nofentries + 1) * sizeof (HilbertIEntry*));
                            memmove(hilbertnode->entries.internal + (item->position + 1), hilbertnode->entries.internal + item->position,
                                    sizeof (hilbertnode->entries.internal[0]) * (hilbertnode->nofentries - item->position));
                        } else if (item->type == FAST_ITEM_TYPE_P) {
                            //here, mode_pointer means that we will have a new entry
                            if (item->position == hilbertnode->nofentries) {
                                hilbertnode->entries.internal = (HilbertIEntry**) lwrealloc(hilbertnode->entries.internal,
                                        (hilbertnode->nofentries + 1) * sizeof (HilbertIEntry*));
                            }
                            hilbertnode->entries.internal[item->position] = (HilbertIEntry*) lwalloc(sizeof (HilbertIEntry));
                            hilbertnode->entries.internal[item->position]->bbox = bbox_create();
                            hilbertnode->entries.internal[item->position]->pointer = item->value.pointer;
                            hilbertnode->nofentries++;
                        } else if (item->type == FAST_ITEM_TYPE_L) {
                            hilbertnode->entries.internal[item->position]->lhv = item->value.lhv;
                        } else {
                            //otherwise, we have to modify the BBOX of an entry
                            if (item->value.bbox == NULL) {
                                //    _DEBUGF(NOTICE, "We have to remove the following entry: %d ", item->position);
                                hilbertnode_remove_entry(hilbertnode, item->position);
                            } else {
                                //    _DEBUG(NOTICE, "We have modified the bbox of this entry");
                                memcpy(hilbertnode->entries.internal[item->position]->bbox, item->value.bbox, sizeof (BBox));
                            }
                        }

                        //we move to the next modification
                        fli = fli->next;
                    }
                } else {
                    //we apply each modification in the modification list
                    while (fli != NULL) {
                        item = fli->item;

                        if (item->position > hilbertnode->nofentries) {
                            _DEBUGF(ERROR, "The list of modification has "
                                    "a position (%d) that will introduce holes: number of"
                                    " elements %d", item->position, hilbertnode->nofentries)
                        }

                        if (item->type == FAST_ITEM_TYPE_H) {
                            hilbertnode->entries.leaf = (REntry**) lwrealloc(hilbertnode->entries.leaf, (hilbertnode->nofentries + 1) * sizeof (REntry*));
                            memmove(hilbertnode->entries.leaf + (item->position + 1), hilbertnode->entries.leaf + item->position,
                                    sizeof (hilbertnode->entries.leaf[0]) * (hilbertnode->nofentries - item->position));
                        } else if (item->type == FAST_ITEM_TYPE_P) {
                            //here, mode_pointer means that we will have a new entry
                            if (item->position == hilbertnode->nofentries) {
                                hilbertnode->entries.leaf = (REntry**) lwrealloc(hilbertnode->entries.leaf,
                                        (hilbertnode->nofentries + 1) * sizeof (REntry*));
                            }
                            hilbertnode->entries.leaf[item->position] = (REntry*) lwalloc(sizeof (REntry));
                            hilbertnode->entries.leaf[item->position]->bbox = bbox_create();
                            hilbertnode->entries.leaf[item->position]->pointer = item->value.pointer;
                            hilbertnode->nofentries++;
                        } else if (item->type == FAST_ITEM_TYPE_L) {
                            _DEBUG(ERROR, "A leaf node hasn't LHV!");
                        } else {
                            //otherwise, we have to modify the BBOX of an entry
                            if (item->value.bbox == NULL) {
                                //    _DEBUGF(NOTICE, "We have to remove the following entry: %d ", item->position);
                                hilbertnode_remove_entry(hilbertnode, item->position);
                            } else {
                                //    _DEBUG(NOTICE, "We have modified the bbox of this entry");
                                memcpy(hilbertnode->entries.internal[item->position]->bbox, item->value.bbox, sizeof (BBox));
                            }
                        }

                        //we move to the next modification
                        fli = fli->next;
                    }
                }
            }

            //_DEBUG(NOTICE, "THE FORMED NODE IS: ");
            //hilbertnode_print(ret, node_page);
            //    print_rnode(ret, rnode_page);

            /*TO-DO may we check the integrity of this node (is it needed?)...*/
        } else {
            //_DEBUG(WARNING, "A node retrieved from the buffer is NULL.");
            //this node does not exist... we have an error if we aren't retrieving this node for flushing operation
            //since it should not happen because other nodes should not point to this deleted node!
            //_DEBUGF(ERROR, "Requested node %d was already removed", rnode_page);
            ret = NULL;
        }
    }

    //_DEBUG(NOTICE, "Retrieval done");

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

/*we definitely remove a rnode from the buffer
 we also decrement the removed bytes in our hash table*/
void fb_free_hashvalue(int node_page, uint8_t index_type) {
    FASTBuffer *buf_entry;
    size_t removed_size = 0;

    //_DEBUGF(NOTICE, "Removing the node %d from the buffer", node_page);

    HASH_FIND_INT(fb, &node_page, buf_entry); //is rnode_page already in the hash?
    if (buf_entry != NULL) {
        HASH_DEL(fb, buf_entry);

        //  _DEBUGF(NOTICE, "The node %d was removed from the buffer", node_page);

        //the key of the hash and the status of the value
        removed_size = size_of_new_hash_element();
        if (buf_entry->status == FAST_STATUS_MOD) {
            if (buf_entry->value.list != NULL) {
                FASTListMod *mods;
                FASTListItem *fli;
                FASTModItem *item;
                mods = buf_entry->value.list;

#ifdef COLLECT_STATISTICAL_DATA
                _cur_mod_node_buffer_num -= buf_entry->nofmod;
#endif
                fli = mods->first;
                while (fli != NULL) {
                    item = fli->item;
                    if (item->type == FAST_ITEM_TYPE_K) {
                        removed_size += size_of_bbox_mod(item->value.bbox);
                    } else if (item->type == FAST_ITEM_TYPE_L) {
                        removed_size += size_of_hilbert_value_mod();
                    } else if (item->type == FAST_ITEM_TYPE_P) {
                        removed_size += size_of_pointer_mod();
                    } else {
                        removed_size += size_of_hole_mod();
                    }
                    //we move to the next modification
                    fli = fli->next;
                }

                flm_destroy(buf_entry->value.list);
                buf_entry->value.list = NULL;
            }
        } else if (buf_entry->status == FAST_STATUS_NEW) {
            if (buf_entry->value.fast_node != NULL) {
                if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                    RNode *rnode = (RNode *) buf_entry->value.fast_node;
#ifdef COLLECT_STATISTICAL_DATA
                    _cur_new_node_buffer_num--;
                    //we decrement the number of modification less one (since one represent that the node when created)
                    _cur_mod_node_buffer_num -= buf_entry->nofmod - 1;
#endif
                    removed_size += size_of_new_node(buf_entry->value.fast_node, index_type);
                    rnode_free(rnode);
                    buf_entry->value.fast_node = NULL;
                } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                    HilbertRNode *hilbertnode = (HilbertRNode *) buf_entry->value.fast_node;
#ifdef COLLECT_STATISTICAL_DATA
                    _cur_new_node_buffer_num--;
                    //we decrement the number of modification less one (since one represent that the node when created)
                    _cur_mod_node_buffer_num -= buf_entry->nofmod - 1;
#endif
                    removed_size += size_of_new_node(buf_entry->value.fast_node, index_type);
                    hilbertnode_free(hilbertnode);
                    buf_entry->value.fast_node = NULL;
                }
            }
        } else {
            if (buf_entry->status == FAST_STATUS_DEL) {
#ifdef COLLECT_STATISTICAL_DATA
                _cur_del_node_buffer_num--;
                //we decrement the number of modification less one (since one represent that the node when removed)
                _cur_mod_node_buffer_num -= buf_entry->nofmod - 1;
#endif
                removed_size += size_of_del_node();
            }
        }

        lwfree(buf_entry);
    } else {

        _nof_unnecessary_flushed_nodes++;
        //_DEBUG(WARNING, "We cannot free a node that do not exist in the hash table... "
        //        "The flushing module wrote a node into disk that was not needed.");
    }
    //we decrement the removed_size from the buffer size
    fast_buffer_size -= removed_size;

    /* only for debug mode
    if (fast_buffer_size != fb_size_buffer()) {
        _DEBUGF(ERROR, "The sizes do not match! Size of the buffer: %d x size of fast_buffer_size %d",
                fb_size_buffer(), fast_buffer_size);
    }
     */

    //_DEBUG(NOTICE, "freeing done");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size = fast_buffer_size;
#endif
}

void fb_destroy_buffer(uint8_t index_type) {

    FASTBuffer *buf_entry, *temp;

    HASH_ITER(hh, fb, buf_entry, temp) {
        HASH_DEL(fb, buf_entry);
        if (buf_entry->status == FAST_STATUS_MOD) {
            if (buf_entry->value.list != NULL) {
                flm_destroy(buf_entry->value.list);
            }
        }
        if (buf_entry->status == FAST_STATUS_NEW) {
            if (buf_entry->value.fast_node != NULL) {
                if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                    rnode_free((RNode *) buf_entry->value.fast_node);
                } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                    hilbertnode_free((HilbertRNode*) buf_entry->value.fast_node);
                }
            }
        }

        lwfree(buf_entry);
    }
    fast_buffer_size = 0;
#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size = fast_buffer_size;
#endif
}

/*
size_t fb_size_buffer() {
    FASTBuffer *buf_entry, *temp;

    size_t size = 0;

    HASH_ITER(hh, fb, buf_entry, temp) {
        //the key of the hash and the status of the value
        size += size_of_new_hash_element();
        if (buf_entry->status == FAST_STATUS_MOD) {
            if (buf_entry->list != NULL) {
                FASTListMod *mods;
                FASTListItem *fli;
                FASTModItem *item;
                mods = buf_entry->list;
                fli = mods->first;
                while (fli != NULL) {
                    item = fli->item;
                    if (item->type == FAST_ITEM_TYPE_K) {
                        size += size_of_bbox_mod(item->bbox);
                    } else {
                        size += size_of_pointer_mod();
                    }
                    //we move to the next modification
                    fli = fli->next;
                }
            }
        } else if (buf_entry->status == FAST_STATUS_NEW) {
            if (buf_entry->rnode != NULL) {
                size += size_of_new_node(buf_entry->rnode);
            }
        } else {
            if (buf_entry->status == FAST_STATUS_DEL) {
                size += size_of_del_node();
            }
        }
    }
    return size;
} */

int fb_get_nofmod(int node_page) {
    FASTBuffer *buf_entry;
    HASH_FIND_INT(fb, &node_page, buf_entry); //is rnode_page already in the hash?
    if (buf_entry == NULL) {
        return 0;
    }
    return buf_entry->nofmod;
}

int fb_get_node_height(int node_page) {
    FASTBuffer *buf_entry;
    HASH_FIND_INT(fb, &node_page, buf_entry); //is rnode_page already in the hash?
    if (buf_entry == NULL) {
        //TODO o problema aqui eh: como saber a altura desse no? pois ele pode escrever nos que nao tem modificações no buffer
        //uma dica de como resolver isso esta no fast_flush_module.h (implementar depois pois nao ha perspectiva de usar o FAST com o HLRU)
        //_DEBUG(WARNING, "This node is not in the buffer");
        return -1;
    }
    return buf_entry->node_height;
}
