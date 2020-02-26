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

#include <liblwgeom.h>
#include <float.h>
#include <stringbuffer.h>

#include "fortree_buffer.h" //basic operations
#include "../libraries/uthash/uthash.h" //for hashing structures
#include "../main/log_messages.h" //for messages
#include "../main/statistical_processing.h" //for collection of statistical data
#include "../main/storage_handler.h" //for i/o operations
#include "../main/io_handler.h"//for the DIRECT

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)

typedef struct _list_item {
    int position;
    REntry *entry;
    struct _list_item *next;
} FORTreeModListItem;

typedef struct {
    int size;
    FORTreeModListItem *head;
} FORTreeListMod;

/*our forb_buffer is a hash map - managed by the UTHASH library*/
typedef struct UpdateBufferTable {
    UT_hash_handle hh;

    int hash_key; //it is the node page
    int modify_count; //number of modifications

    /*the height of the node (this is not counted in the buffer size)*
     * we need this value for the HLRU buffer 
     the overhead introduced here is very low because we need to know the height of the node
     * and this solution is better than to (i) create a new structure to store this value
     * or (ii) store this value into the Node structure */
    int node_height;

    uint8_t status; //new, mod, or del
    FORTreeListMod *list; //this is NULL when status is DEL    
} UpdateBufferTable;

static size_t forb_buffer_size = 0; //size in bytes of our buffer
static UpdateBufferTable *forb = NULL;

/*this struct is to manage the warm node list in order to perform flushing operation*/
typedef struct {
    int n; //total number of elements in the list
    int current_position; //the current position to include new elements
    int *list; //the array of the nodes
} WarmNodeList;

static WarmNodeList *wml = NULL;

/*functions to manage the list of modifications*/
static FORTreeListMod *forb_flm_init(void);
static void forb_flm_append(FORTreeListMod *flm, REntry *rentry, int position);
static void forb_flm_destroy(FORTreeListMod *flm);

FORTreeListMod *forb_flm_init() {
    FORTreeListMod *flm = (FORTreeListMod*) lwalloc(sizeof (FORTreeListMod));
    flm->head = NULL;
    flm->size = 0;
    return flm;
}

void forb_flm_append(FORTreeListMod *flm, REntry *rentry, int position) {
    FORTreeModListItem *fli = lwalloc(sizeof (FORTreeModListItem));
    fli->entry = rentry;
    fli->position = position;
    fli->next = NULL;

    if (flm->head == NULL) {
        flm->head = fli;
    } else {
        FORTreeModListItem *last = flm->head;
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = fli;
    }

    flm->size++;
}

/* free the FLM*/
void forb_flm_destroy(FORTreeListMod *flm) {
    FORTreeModListItem *current;
    while (flm->head != NULL) {
        current = flm->head;
        flm->head = current->next;

        if (current->entry != NULL) {
            lwfree(current->entry->bbox);
            lwfree(current->entry);
        }

        lwfree(current);
    }
    lwfree(flm);
}

/* extract the required sizes (like in the FAST) */
static size_t size_of_new_hash_element(void);
static size_t size_of_mod_rnode(REntry *entry);
static size_t size_of_del_rnode(void);

size_t size_of_new_hash_element() {
    //size of the hash key, status and number of modifications
    return sizeof (int) + sizeof (uint8_t) + sizeof (int);
}

size_t size_of_mod_rnode(REntry *entry) {
    if (entry == NULL) {
        //size of position
        return sizeof (int);
    } else {
        //size of position and rentry
        return sizeof (int) +rentry_size();
    }
}

size_t size_of_del_rnode() {
    //there is no required size (confirm it)
    return 0;
}

/*this function is responsible to update the warm list in order to aid in flushing operations*/
static void update_warm_list(FORTreeSpecification *spec, int node_page);

void update_warm_list(FORTreeSpecification *spec, int rnode_page) {
    int wlcapacity;
    unsigned total;
    total = HASH_COUNT(forb);
    wlcapacity = (int) (total * (spec->ratio_flushing / 100.0));
    if (wml == NULL) {
        //is this the moment to create a warm node list?
        if (wlcapacity >= 1) {
            wml = (WarmNodeList*) lwalloc(sizeof (WarmNodeList));
            wml->current_position = 0;
            wml->list = (int*) lwalloc(sizeof (int) * wlcapacity);
            wml->n = wlcapacity;

            //we update the current position and its corresponding page
            wml->list[wml->current_position % wml->n] = rnode_page;
            wml->current_position++;
        }
    } else {
        //we have a warm node list, then we check if we need more space
        if (wlcapacity > wml->n) {
            wml->list = (int*) lwrealloc(wml->list, sizeof (int)*wlcapacity);
            wml->n = wlcapacity;
        }
        //we update the current position and its corresponding page
        wml->list[wml->current_position % wml->n] = rnode_page;
        wml->current_position++;
    }
}

/*it only creates a new entry in the hash table*/
void forb_create_new_rnode(const SpatialIndex *base, FORTreeSpecification *spec, int new_node_page, int height) {
    UpdateBufferTable *buf_entry;
    size_t required_size = 0;
    HASH_FIND_INT(forb, &new_node_page, buf_entry); //is new_node_page already in the hash?

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = size_of_new_hash_element();
    } else {
        //if this node was previously removed, then we recreate it
        if (buf_entry->status == FORTREE_STATUS_DEL) {
            required_size = 0;
#ifdef COLLECT_STATISTICAL_DATA
            _cur_del_node_buffer_num--;
         //   _del_node_buffer_num--;
#endif
        } else {
            _DEBUGF(ERROR, "This node (%d) already exists in the update node table!",
                    new_node_page);
        }
        return;
    }
    //if we do not have space, we execute the flushing
    if (required_size > 0 && spec->buffer_size < (required_size + forb_buffer_size)) {
        //(NOTICE, "Performing flushing");
        forb_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(forb, &new_node_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = size_of_new_hash_element();
        }

        //(NOTICE, "Flushing completed");
    }

    /*then we put it into our buffer because we have space now =)*/
    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (UpdateBufferTable*) lwalloc(sizeof (UpdateBufferTable));
        buf_entry->hash_key = new_node_page;
        buf_entry->modify_count = 0;
        buf_entry->node_height = height;
        HASH_ADD_INT(forb, hash_key, buf_entry); //we add it         
    }
    buf_entry->status = FORTREE_STATUS_NEW;
    buf_entry->list = forb_flm_init();
    buf_entry->modify_count++;

    //we increment the forb_buffer_size
    forb_buffer_size += required_size;

#ifdef COLLECT_STATISTICAL_DATA
    _cur_new_node_buffer_num++;
    _new_node_buffer_num++;

    _cur_buffer_size = forb_buffer_size;
#endif
}

void forb_put_mod_rnode(const SpatialIndex *base, FORTreeSpecification *spec,
        int rnode_page, int position, REntry *entry, int height) {
    UpdateBufferTable *buf_entry;
    size_t required_size = 0;

    HASH_FIND_INT(forb, &rnode_page, buf_entry); //is rnode_page already in the hash?

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = size_of_new_hash_element() + size_of_mod_rnode(entry);
    } else {
        if (buf_entry->status == FORTREE_STATUS_DEL) {
            _DEBUG(ERROR, "Invalid operation! You are trying to put an element in a removed node!");
            return;
        } else {
            required_size = size_of_mod_rnode(entry);
        }
    }

    //if we do not have space, we execute the flushing
    if (spec->buffer_size < (required_size + forb_buffer_size)) {
        //(NOTICE, "Performing flushing");
        forb_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(forb, &rnode_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = size_of_new_hash_element() + size_of_mod_rnode(entry);
        }

        //(NOTICE, "Flushing completed");
    }

    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (UpdateBufferTable*) lwalloc(sizeof (UpdateBufferTable));
        buf_entry->hash_key = rnode_page;
        buf_entry->status = FORTREE_STATUS_MOD;
        buf_entry->list = forb_flm_init();
        buf_entry->modify_count = 0;
        buf_entry->node_height = height;
        HASH_ADD_INT(forb, hash_key, buf_entry); //we add it        
    }
    buf_entry->modify_count++;
    //when entry is NULL, then we consider that entry was removed (the entry position was removed)
    //we append the mod list
    forb_flm_append(buf_entry->list, entry, position);

    //(NOTICE, "Adicionou no buffer");

    //we increment the buffer_size
    forb_buffer_size += required_size;

    //(NOTICE, "Atualizando o warm node list");

    /*we update the warm node list*/
    update_warm_list(spec, rnode_page);

    //(NOTICE, "Feito");

#ifdef COLLECT_STATISTICAL_DATA
    _cur_mod_node_buffer_num++;
    _mod_node_buffer_num++;

    _cur_buffer_size = forb_buffer_size;
#endif
}

void forb_put_del_rnode(const SpatialIndex *base, FORTreeSpecification *spec, int rnode_page, int height) {
    UpdateBufferTable *buf_entry;
    size_t required_size = 0;

    HASH_FIND_INT(forb, &rnode_page, buf_entry); //is rnode_page already in the hash?

    /*we firstly compute the size in order to know if
     * it fits in our buffer or if we need to execute the flushing*/
    if (buf_entry == NULL) {
        required_size = size_of_new_hash_element() + size_of_del_rnode();
    } else {
        required_size = size_of_del_rnode();
    }

    //if we do not have space, we execute the flushing
    if (required_size > 0 && spec->buffer_size < (required_size + forb_buffer_size)) {
        //(NOTICE, "Performing flushing");
        forb_flushing(base, spec);

        /*we have to execute again a search in the hash table 
        since this node can be flushed by the flushing operation*/
        HASH_FIND_INT(forb, &rnode_page, buf_entry);
        /*we have to add the hashing key size if a new element will be created again*/
        if (buf_entry == NULL) {
            required_size = size_of_new_hash_element() + size_of_del_rnode();
        }

        //(NOTICE, "Flushing completed");
    }

    if (buf_entry == NULL) {
        //in the negative case, we have to add a new entry
        buf_entry = (UpdateBufferTable*) lwalloc(sizeof (UpdateBufferTable));
        buf_entry->hash_key = rnode_page;
        buf_entry->status = FORTREE_STATUS_DEL;
        buf_entry->list = NULL;
        buf_entry->modify_count = 0;
        buf_entry->node_height = height;
        HASH_ADD_INT(forb, hash_key, buf_entry); //we add it        
    } else {
        size_t removed_size = 0;
#ifdef COLLECT_STATISTICAL_DATA
        if (buf_entry->status == FORTREE_STATUS_NEW) {
            _cur_new_node_buffer_num--;
           // _new_node_buffer_num--;
        }
#endif

        //we need to del the modification list if this node received modification in the past
        //as a consequence we will have some new space
        buf_entry->status = FORTREE_STATUS_DEL;
        //we will free some space from buffer here
        if (buf_entry->list != NULL) {
            FORTreeListMod *mods;
            FORTreeModListItem *item;
            mods = buf_entry->list;
            item = mods->head;
            while (item != NULL) {
                //we free this size for this entry
                removed_size += size_of_mod_rnode(item->entry);
                //we move to the next modification
                item = item->next;
            }

            forb_flm_destroy(buf_entry->list);
            buf_entry->list = NULL;
        }
        required_size -= removed_size;
    }
    buf_entry->modify_count++;

    //we increment the forb_buffer_size
    forb_buffer_size += required_size;

    //we update the warm node list
    update_warm_list(spec, rnode_page);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_del_node_buffer_num++;
    _del_node_buffer_num++;

    _cur_buffer_size = forb_buffer_size;
#endif
}

static int contains(int *vec, int n, int v);

int contains(int *vec, int n, int v) {
    int i;
    for (i = 0; i < n; i++) {
        if (vec[i] == v)
            return i;
    }
    return -1;
}

static int forb_get_node_height(int rnode_page);

int forb_get_node_height(int rnode_page) {
    UpdateBufferTable *buf_entry;

    HASH_FIND_INT(forb, &rnode_page, buf_entry); //is rnode_page already in the hash?
    if (buf_entry != NULL)
        return buf_entry->node_height;
    else {
        _DEBUG(ERROR, "This node has not a height. Some problem happened in the management of the buffer.");
        return -1;
    }
}

/*we retrieve the most recent version of a RNODE by considering possible modification in the buffer
 after the call, we return the most recent version of the request node (rnode_page)*/
RNode *forb_retrieve_rnode(const SpatialIndex *base, int rnode_page, int height) {
    UpdateBufferTable *buf_entry;
    RNode *ret = NULL;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    HASH_FIND_INT(forb, &rnode_page, buf_entry); //is rnode_page already in the hash?
    if (buf_entry != NULL) {
        //if this node is in the buffer, we check if it is a newly created node
        //or a node with modifications
        if (buf_entry->status == FORTREE_STATUS_MOD ||
                buf_entry->status == FORTREE_STATUS_NEW) {
            //if it is a modified node, we need to apply the modification list
            FORTreeListMod *mods;
            FORTreeModListItem *item;
            if (buf_entry->status == FORTREE_STATUS_MOD) {
                //we firstly read this node from the storage device
                ret = get_rnode(base, rnode_page, height);
            } else {
                //otherwise we create this node now
                ret = rnode_create_empty();
            }

            mods = buf_entry->list;
            if (mods == NULL || mods->head == NULL || mods->size == 0) {
                //in this case, this is an empty created node
                return ret;
                //_DEBUG(ERROR, "It should has a modification list, "
                //        "but it is currently empty... something wrong in the buffer");
                //return NULL;
            }
            //we apply each modification in the modification list
            item = mods->head;
            while (item != NULL) {
                if (item->position > ret->nofentries) {
                    _DEBUGF(ERROR, "The list of modification has "
                            "a position (%d) that will introduce holes: number of"
                            " elements %d", item->position, ret->nofentries)
                }

                //we are adding/modifying an entry or removing?
                if (item->entry == NULL) {
                    rnode_remove_rentry(ret, item->position);
                } else {
                    //here we are adding
                    if (item->position == ret->nofentries) {
                        rnode_add_rentry(ret, rentry_clone(item->entry));
                    } else {
                        //otherwise, we have to modify it
                        ret->entries[item->position]->pointer = item->entry->pointer;
                        memcpy(ret->entries[item->position]->bbox, item->entry->bbox, sizeof (BBox));
                    }
                }

                //we move to the next modification
                item = item->next;
            }

            /*may we check the integrity of this node...*/
            //TO-DO
        } else {
            //this node does not exist... we have an error if we aren't retrieving this node for flushing operation
            //since it should not happen because other nodes should not point to this deleted node!
            ret = NULL;
        }
    } else {
        //this node is not in the buffer
        //therefore, we have to return the node stored in the secondary memory
        ret = get_rnode(base, rnode_page, height);
    }

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

/*we definitely remove a rnode from the buffer*/
void forb_free_hashvalue(const FORTreeSpecification *fr, int rnode_page) {
    UpdateBufferTable *buf_entry;
    size_t removed_size = 0;

    HASH_FIND_INT(forb, &rnode_page, buf_entry); //is rnode_page already in the hash?
    if (buf_entry != NULL) {
        HASH_DEL(forb, buf_entry);
        removed_size += size_of_new_hash_element();

        if (buf_entry->list != NULL) {
            FORTreeListMod *mods;
            FORTreeModListItem *item;
            mods = buf_entry->list;

            item = mods->head;
            while (item != NULL) {
                removed_size += size_of_mod_rnode(item->entry);
                //we move to the next modification
                item = item->next;
            }

            forb_flm_destroy(buf_entry->list);
        }
#ifdef COLLECT_STATISTICAL_DATA
        if (buf_entry->status == FORTREE_STATUS_NEW) {
            _cur_new_node_buffer_num--;
            //we decrement the number of modification less one (since one represent that the node when created)
            _cur_mod_node_buffer_num -= buf_entry->modify_count - 1;
        } else if (buf_entry->status == FORTREE_STATUS_DEL) {
            _cur_del_node_buffer_num--;
            //we decrement the number of modification less one (since one represent that the node when removed)
            _cur_mod_node_buffer_num -= buf_entry->modify_count - 1;
        } else if (buf_entry->status == FORTREE_STATUS_MOD) {
            _cur_mod_node_buffer_num -= buf_entry->modify_count;
        }
#endif
        lwfree(buf_entry);
    } else {
        _DEBUG(WARNING, "We cannot free a node that do not exist in the hash table..."
                "Probably the flushing module write a node that was not needed into disk");
    }
    //we decrement the removed_size from the buffer size
    forb_buffer_size -= removed_size;

    //we also update the warm node list by removing the corresponding element
    if (wml != NULL) {
        int e;
        unsigned total = HASH_COUNT(forb);

        e = contains(wml->list, wml->n, rnode_page);
        //if we remove the last element we just update the current_position
        if (e + 1 == wml->n) {
            wml->current_position--;
        } else if (e != -1 && e < wml->n) {
            //we have to remove it from the wml            
            wml->list[e] = -1;
        }

        //we check if we have to decrease the size of the wml
        if (((int) (total * (fr->ratio_flushing / 100))) < wml->n) {
            if (e != -1)
                memmove(wml->list + e, wml->list + (e + 1), sizeof (int) * (wml->n - e - 1));
            else
                memmove(wml->list + 0, wml->list + (0 + 1), sizeof (int) * (wml->n - 1));
            wml->n--;
        }
    }

#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size = forb_buffer_size;
#endif
}

void forb_destroy_buffer() {
    UpdateBufferTable *buf_entry, *temp;

    HASH_ITER(hh, forb, buf_entry, temp) {
        HASH_DEL(forb, buf_entry);
        if (buf_entry->list != NULL)
            forb_flm_destroy(buf_entry->list);
        lwfree(buf_entry);
    }
    forb_buffer_size = 0;

    if (wml != NULL) {
        lwfree(wml->list);
        lwfree(wml);
        wml = NULL;
    }
#ifdef COLLECT_STATISTICAL_DATA
    _cur_buffer_size = forb_buffer_size;
#endif
}

static int id_sort(UpdateBufferTable *a, UpdateBufferTable *b);

int id_sort(UpdateBufferTable *a, UpdateBufferTable *b) {
    return (a->hash_key - b->hash_key);
}

void forb_flushing(const SpatialIndex *base, FORTreeSpecification *spec) {
    int **flushing_units; //we store all possible flushing unit   

    int fu_num; //number of flushing units (this is the index)
    int fu_capacity; //number of nodes inside of flushing units (this is the index)

    int *fuc; //the selected flushing unit
    int n_fuc = -1; //number of nodes in the fuc
    int *node_heights; //the height of the nodes in fuc

    bool chosen_fuc; //we have chosen a flushing unit?

    int aux_n_fuc;
    int max_modify_count;
    int cur_modify_count;
    unsigned total = HASH_COUNT(forb);
    int n;

    int aux, aux2;
    bool inside;
    int entry;
    RNode *node;

    UpdateBufferTable *s;

    uint8_t *buf, *loc;
    size_t buf_size;

    // stringbuffer_t *sb;
    // char *print;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();

    _flushing_num++;
#endif

    /*
    sb = stringbuffer_create();
    stringbuffer_append(sb, "The current elements of the wml are (");
    //seeing the wml
    for (n = 0; n < wml->n; n++) {
        stringbuffer_aprintf(sb, "%d, ", wml->list[n]);
    }
    print = stringbuffer_getstringcopy(sb);
    stringbuffer_destroy(sb);

    _DEBUGF(NOTICE, "%s", print); */

    /*we sort the hash table in order to guarantee the sequential write of the nodes*/
    HASH_SORT(forb, id_sort);
    /*the possible flushing units is equal to n
     this corresponds to a fast ceil operation (to round up) */
    n = (total + spec->flushing_unit_size - 1) / spec->flushing_unit_size;
    /*we alloc the array of arrays*/
    flushing_units = (int**) lwalloc(sizeof (int*)*n);

    //row (that is, a flushing unit)
    fu_num = -1;
    //column (that is, a value/position of a flushing unit)
    fu_capacity = spec->flushing_unit_size;

    //we alloc the flushing unit that will be THE ONE
    fuc = (int*) lwalloc(sizeof (int)*spec->flushing_unit_size);
    //we have not chosen a flushing unit yet
    chosen_fuc = false;
    //iterating the hash after the sorting by the node_id
    //the goal here is to organize all the possible flushing units
    for (s = forb; s != NULL; s = (UpdateBufferTable*) (s->hh.next)) {
        //we alloc a new flushing unit if the capacity of the current flushing unit is full
        if (fu_capacity == spec->flushing_unit_size) {
            fu_num++;
            flushing_units[fu_num] = (int*) lwalloc(sizeof (int)*spec->flushing_unit_size);
            fu_capacity = 0;
        }
        flushing_units[fu_num][fu_capacity] = s->hash_key;
        fu_capacity++;
    }
    //we fill the last flushing unit with -1 if it is a incomplete flushing unit
    while (fu_capacity < spec->flushing_unit_size) {
        flushing_units[fu_num][fu_capacity] = -1;
        fu_capacity++;
    }
    if (fu_num != (n - 1) || fu_capacity != spec->flushing_unit_size) {
        _DEBUGF(ERROR, "Wow, the number of flushing units do not match!. "
                "The number of flushing units is %d and the last flushing unit index is %d."
                "The capacity of a flushing unit is %d and the configuration is equal to %d.",
                n, fu_num, fu_capacity, spec->flushing_unit_size);
        return;
    }
    max_modify_count = -1;
    //for each flushing unit (the number of flushing units is equal to i)
    for (aux = 0; aux <= fu_num; aux++) {
        /*we have to check if there is a node of this flushing unit 
         inside WML*/
        inside = false;
        for (aux2 = 0; aux2 < fu_capacity; aux2++) {
            entry = contains(wml->list, wml->n, flushing_units[aux][aux2]);
            /*if we found the node of this flushing unit in the WML, 
            we do not consider this flushing unit */
            if (entry != -1) {
                inside = true;
                break;
            }
        }
        //if this flushing does not have recent modifications, we consider it
        if (!inside) {
            cur_modify_count = 0;
            aux_n_fuc = 0;
            //we calculate the modify_count of this flushing unit
            for (aux2 = 0; aux2 < fu_capacity; aux2++) {
                if (flushing_units[aux][aux2] != -1) {
                    HASH_FIND_INT(forb, &flushing_units[aux][aux2], s);
                    if (s == NULL) {
                        _DEBUGF(ERROR, "The node %d of a flushing unit is not "
                                "in the buffer...", flushing_units[aux][aux2]);
                    }
                    cur_modify_count += s->modify_count;
                    aux_n_fuc++;
                }
            }
            //we have found a good flushing unit to be used
            if (cur_modify_count > max_modify_count) {
                //we copy its content into fuc
                memcpy(fuc, flushing_units[aux], sizeof (int)*fu_capacity);
                max_modify_count = cur_modify_count;
                n_fuc = aux_n_fuc;
                //we found a good one
                chosen_fuc = true;
            }
        }
    }

    if (n_fuc == 0) {
        buf_size = fu_capacity * base->gp->page_size;
    } else {
        /*we write in the flash, all nodes of the chosen flushing unit*/
        buf_size = n_fuc * base->gp->page_size;
    }

    if (base->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, base->gp->page_size, buf_size)) {
            _DEBUG(ERROR, "Allocation failed at forb_flushing");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(buf_size);
    }

    loc = buf;

    /*if we did not find a good flushing unit, 
     * we get the first one since we need space in our buffer!*/
    if (!chosen_fuc) {
        n_fuc = fu_capacity;
        for (aux = 0; aux < n_fuc; aux++) {
            if (flushing_units[0][aux] != -1) {
                fuc[aux] = flushing_units[0][aux];
            } else {
                n_fuc--;
            }
        }
    }

    node_heights = (int*) lwalloc(sizeof (int) * n_fuc);

    for (aux = 0; aux < n_fuc; aux++) {
        node_heights[aux] = forb_get_node_height(fuc[aux]);
        node = forb_retrieve_rnode(base, fuc[aux], node_heights[aux]);
        rnode_serialize(node, loc);
        if (node != NULL)
            rnode_free(node);
        loc += base->gp->page_size;
    }
    //we write in batch
    storage_write_pages(base, fuc, buf, node_heights, n_fuc);

#ifdef COLLECT_STATISTICAL_DATA
    _flushed_nodes_num += n_fuc;
#endif

    //we remove them from the buffer
    for (aux = 0; aux < n_fuc; aux++) {
        forb_free_hashvalue(spec, fuc[aux]);
    }
    //free used memory
    lwfree(fuc);
    lwfree(node_heights);
    for (aux = 0; aux <= fu_num; aux++) {
        lwfree(flushing_units[aux]);
    }
    lwfree(flushing_units);
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

void forb_flushing_all(const SpatialIndex *base, FORTreeSpecification *spec) {
    unsigned total = HASH_COUNT(forb);
    int *node_pages;
    int *node_heights;
    int n, i;
    RNode *node;
    UpdateBufferTable *s;
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

    if (total == 0) {
        return;
    }

    HASH_SORT(forb, id_sort);

    /*we write in the flash all nodes of the buffer*/
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
    //iterating the hash after the sorting by the node_id
    for (s = forb; s != NULL; s = (UpdateBufferTable*) (s->hh.next)) {
        node = forb_retrieve_rnode(base, s->hash_key, s->node_height);
        rnode_serialize(node, loc);
        if (node != NULL)
            rnode_free(node);
        loc += base->gp->page_size;
        node_pages[n] = s->hash_key;
        node_heights[n] = s->node_height;
        n++;
    }
    if (n != total) {
        _DEBUG(ERROR, "The number of serialized nodes does not match "
                "with the number of nodes in the buffer at forb_flushing_all");
    }
    //we write in batch
    storage_write_pages(base, node_pages, buf, node_heights, n);

    //we remove them from the buffer
    for (i = 0; i < n; i++) {
        forb_free_hashvalue(spec, node_pages[i]);
    }
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
#endif
}
