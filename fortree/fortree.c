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

#include <float.h>

#include "../libraries/uthash/uthash.h"
#include "fortree.h"
#include "fortree_buffer.h"
#include "fornode_stack.h"
#include "fortree_nodeset.h"

#include "../main/header_handler.h" //for header storage

#include "../main/log_messages.h"
#include "../main/math_util.h"

#include "../main/storage_handler.h" //for the tree height update

#include "../main/statistical_processing.h"

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)

#undef uthash_fatal
#define uthash_fatal(msg) _DEBUG(ERROR, msg)

/* the overflow node table, which is a hash table indexed by the node_id*/
typedef struct OverflowNodeTable {
    int node_id; //the id of the P-node
    int k; //the number of O-node of the node_id
    int tsc; //the number of searches done in these O-nodes
    int *o_nodes; //the array storing the ids of the O-nodes
    UT_hash_handle hh;
} OverflowNodeTable;

static OverflowNodeTable *ont = NULL;

/* used for the remotion algorithm */
typedef struct {
    RNode *chosen_node;
    int chosen_node_add;
    int entry_chosen_node;
    FORNodeSet *s;
    RNode *p_node;
    int p_node_add;
} ChooseLeaf;

/*auxiliary functions*/

/*function to calculate the BBOX of a p-node and its o-nodes*/
static BBox *fortree_union_allnodes(RNode *p, FORNodeSet *s);

static void fortree_mergeback(FORTree *fr, const FORNodeSet *src, FORNodeSet *dest, const RNode *oldp, RNode *p, int p_node, int level);
static FORNodeSet *fortree_add_element(FORTree *fr, int level, int p_node, RNode *p, REntry *e, bool *mb);
static SpatialIndexResult *fortree_recursive_search(FORTree *fr, int node_page, const BBox *query, uint8_t predicate, int height, SpatialIndexResult *result);
static RNode *fortree_choose_node(FORTree *fr, REntry *input, int level, FORNodeStack *stack, int *chosen_address);
static FORNodeSet *fortree_adjust_tree(FORTree *fr, RNode *l, FORNodeSet *s, bool *mb, int l_level, FORNodeStack *stack);
static ChooseLeaf *fortree_choose_leaf(FORTree *fr, int p_node_add, REntry *to_remove, int height, FORNodeStack *stack, ChooseLeaf *cl);
static void fortree_condense_tree(FORTree *fr, ChooseLeaf *cl, FORNodeStack *stack);
static bool fortree_remove_entry(FORTree *fr, REntry *to_remove);
static SpatialIndexResult *fortree_search(FORTree *fr, const BBox *query, uint8_t predicate);
static void fortree_insert_entry(FORTree *fr, REntry *input, int level);
static BBox *fortree_union_allnodes(RNode *p, FORNodeSet *s);

BBox *fortree_union_allnodes(RNode *p, FORNodeSet *s) {
    BBox *ret;
    BBox *tmp;
    int i;
    ret = rnode_compute_bbox(p);
    if (s != NULL) {
        for (i = 0; i < s->n; i++) {
            tmp = rnode_compute_bbox(s->o_nodes[i]);
            bbox_increment_union(tmp, ret);
            lwfree(tmp);
        }
    }
    return ret;
}

/*recursive function to query a for-tree - it is the same algorithm of the R-tree
 but considering the O-nodes*/
SpatialIndexResult *fortree_recursive_search(FORTree *fr, int node_page,
        const BBox *query, uint8_t predicate, int height, SpatialIndexResult *result) {
    RNode *node;
    int node_p;
    int i, j; //counter for the FOR
    int k; //number of nodes to traverse (p-node + possible o-nodes)
    uint8_t p;
    OverflowNodeTable *hash_entry;

    node = rnode_clone(fr->current_node);

    //we check if this node has o-node or not (an O-NODE only points to a P-NODE)
    HASH_FIND_INT(ont, &node_page, hash_entry);
    if (hash_entry != NULL) {
        //if this node has o-nodes, we increment the tsc value
        hash_entry->tsc++;
        k = hash_entry->k + 1;
       // _DEBUGF(NOTICE, "The value of tsc for the node %d is %d", node_page, hash_entry->tsc);
    } else {
        k = 1;
    }

    if (height != 0) {
        for (j = 0; j < k; j++) {
            //if we have to retrieve a node, we have to update the current node
            if (j > 0) {
                rnode_free(fr->current_node);
                fr->current_node = forb_retrieve_rnode(&fr->base, hash_entry->o_nodes[j - 1], height);
#ifdef COLLECT_STATISTICAL_DATA
                _visited_int_node_num++;
                insert_reads_per_height(height, 1);
#endif
                rnode_copy(node, fr->current_node);
            }

            for (i = 0; i < fr->current_node->nofentries; i++) {
                p = predicate;

                /* there are two cases here:
             1 - if the predicate is not inside
                 then, we must check if there is an intersection
             2 - otherwise, the predicate is inside,
                 then, we must check if the query object is inside of the entry
             This is evaluated since if the query object is inside of the entry, 
                 * then all the children of this entry will also be contained in the query
             That is, it minimizes the selected paths */
                if (p != INSIDE_OR_COVEREDBY)
                    p = INTERSECTS;

#ifdef COLLECT_STATISTICAL_DATA
                _processed_entries_num++;
#endif

                if (bbox_check_predicate(query, fr->current_node->entries[i]->bbox, p)) {
                    //we get the node in which the entry points to
                    node_p = fr->current_node->entries[i]->pointer;
                    fr->current_node = forb_retrieve_rnode(&fr->base, fr->current_node->entries[i]->pointer, height - 1);

#ifdef COLLECT_STATISTICAL_DATA
                    if (height - 1 != 0) {
                        //we visited one internal node, then we add it
                        _visited_int_node_num++;
                    } else {
                        //we visited one leaf node
                        _visited_leaf_node_num++;
                    }
                    insert_reads_per_height(height - 1, 1);
#endif
                    result = fortree_recursive_search(fr, node_p, query, predicate, height - 1, result);

                    /*after to traverse this child, we need to back
                     * the reference of the current_node for the original one*/
                    rnode_copy(fr->current_node, node);
                }
            }
        }
    } else {
        for (j = 0; j < k; j++) {
            if (j > 0) {
                rnode_free(fr->current_node);
                fr->current_node = forb_retrieve_rnode(&fr->base, hash_entry->o_nodes[j - 1], height);
#ifdef COLLECT_STATISTICAL_DATA                
                _visited_leaf_node_num++;
                insert_reads_per_height(height, 1);
#endif
                rnode_copy(node, fr->current_node);
            }
            for (i = 0; i < fr->current_node->nofentries; i++) {
#ifdef COLLECT_STATISTICAL_DATA
                _processed_entries_num++;
#endif
                /*  * We employ MBRs relationships, like defined in: 
                 * CLEMENTINI, E.; SHARMA, J.; EGENHOFER, M. J. Modelling topological spatial relations:
 Strategies for query processing. Computers & Graphics, v. 18, n. 6, p. 815–822, 1994.*/
                if (bbox_check_predicate(query, fr->current_node->entries[i]->bbox, predicate)) {
                    spatial_index_result_add(result, fr->current_node->entries[i]->pointer);
                }
            }
        }
    }
    rnode_free(node);
    return result;
}

/*this function inserts the O-nodes into the corresponding P-node and 
 * sets the "dest" as the remaining O-nodes to be inserted in the parent P-node*/
void fortree_mergeback(FORTree *fr, const FORNodeSet *src, FORNodeSet *dest,
        const RNode *oldp, RNode *p, int p_node, int height) {
    int i, j, position, insert_page, count;
    const RNode *current;
    RNode *inserting;

#ifdef COLLECT_STATISTICAL_DATA    
    _merge_back_num++;
#endif

    count = 0;

    inserting = p;
    insert_page = p_node;

    //for each node (including the P-node and its O-nodes)
    for (i = 0; i < (src->n + 1); i++) {
        if (i == 0)
            current = oldp;
        else {
            current = src->o_nodes[i - 1];
        }
        j = 0;
        //for each entry of the current node
        while (j < current->nofentries) {
            //we have space in the inserting node
            if ((height == 0 && inserting->nofentries < fr->spec->max_entries_leaf_node)
                    || (height > 0 && inserting->nofentries < fr->spec->max_entries_int_node)) {
                rnode_add_rentry(inserting, rentry_clone(current->entries[j]));

                position = inserting->nofentries - 1;
                forb_put_mod_rnode(&fr->base, fr->spec, insert_page, position,
                        rentry_clone(current->entries[j]), height);

#ifdef COLLECT_STATISTICAL_DATA    
                if (height > 0)
                    _written_int_node_num++;
                else
                    _written_leaf_node_num++;
                insert_writes_per_height(height, 1);
#endif

                //go to the next entry
                j++;
            } else {
                //otherwise, we need to insert it in other node
                count++;
                if (count > dest->n) {
                    dest->n++;
                    dest->o_nodes = (RNode**) lwrealloc(dest->o_nodes, sizeof (RNode*) * dest->n);
                    dest->o_nodes[count - 1] = rnode_create_empty();
                    dest->o_nodes_pages = (int*) lwrealloc(dest->o_nodes_pages, sizeof (int) * dest->n);
                }
                inserting = dest->o_nodes[count - 1];
                insert_page = src->o_nodes_pages[count - 1];
                dest->o_nodes_pages[count - 1] = insert_page;
            }
        }
    }
}

/* we always call this function in order to add a new entry in a FORNODE 
 we firstly try to add the current element in the P-node
 but, if its capacity is full, we put in the first O-node with some space (we return the o-nodes of this node).
 If a mergeback operation was done, then we set "mb" as true 
 */
FORNodeSet *fortree_add_element(FORTree *fr, int height, int p_node, RNode *p, REntry *e, bool *mb) {
    int position;
    FORNodeSet *ret;
    if ((height == 0 && p->nofentries < fr->spec->max_entries_leaf_node)
            || (height > 0 && p->nofentries < fr->spec->max_entries_int_node)) {
//        _DEBUG(NOTICE, "adicionando");
        rnode_add_rentry(p, e);        
        position = p->nofentries - 1;        
        //there is space, then we add the new entry directly
        forb_put_mod_rnode(&fr->base, fr->spec, p_node, position, rentry_clone(e), height);
        
#ifdef COLLECT_STATISTICAL_DATA    
        if (height > 0)
            _written_int_node_num++;
        else
            _written_leaf_node_num++;
        insert_writes_per_height(height, 1);
#endif
        *mb = false;
        ret = NULL;
 //       _DEBUG(NOTICE, "Adicionou");
    } else {
        OverflowNodeTable *hash_entry;
        HASH_FIND_INT(ont, &p_node, hash_entry);

        //this p-node has not o-nodes
        if (hash_entry == NULL) {
            //then create a new hash_entry
            hash_entry = (OverflowNodeTable*) lwalloc(sizeof (OverflowNodeTable));
            hash_entry->k = 1; //number of o-nodes
            hash_entry->tsc = 0; //number of searches in this o-node
            hash_entry->node_id = p_node; //the p-node
            hash_entry->o_nodes = (int*) lwalloc(sizeof (int) * hash_entry->k);

            //create a new O-node for the p_node
            hash_entry->o_nodes[hash_entry->k - 1] = rtreesinfo_get_valid_page(fr->info);
            //put this node into the buffer
            forb_create_new_rnode(&fr->base, fr->spec, hash_entry->o_nodes[hash_entry->k - 1], height);
            //put the entry into this node (in the buffer)
            position = 0;
            forb_put_mod_rnode(&fr->base, fr->spec, hash_entry->o_nodes[hash_entry->k - 1], position, rentry_clone(e), height);

#ifdef COLLECT_STATISTICAL_DATA    
            if (height > 0) {
                _written_int_node_num++;
            } else {
                _written_leaf_node_num++;
            }
            insert_writes_per_height(height, 1);
#endif

            ret = fortree_nodeset_create(1);
            ret->o_nodes[0] = rnode_create_empty();
            ret->o_nodes_pages[0] = hash_entry->o_nodes[hash_entry->k - 1];
            rnode_add_rentry(ret->o_nodes[0], e);

            *mb = false;

            HASH_ADD_INT(ont, node_id, hash_entry);
        } else {
            int i;
            bool inserted = false;

            ret = fortree_nodeset_create(hash_entry->k);

            for (i = 0; i < hash_entry->k; i++) {
                ret->o_nodes[i] = forb_retrieve_rnode(&fr->base, hash_entry->o_nodes[i], height);
                ret->o_nodes_pages[i] = hash_entry->o_nodes[i];
#ifdef COLLECT_STATISTICAL_DATA       
                if (height == 0)
                    _visited_leaf_node_num++;
                else
                    _visited_int_node_num++;
                insert_reads_per_height(height, 1);
#endif
                if (!inserted && ((height == 0 && ret->o_nodes[i]->nofentries < fr->spec->max_entries_leaf_node)
                        || (height > 0 && ret->o_nodes[i]->nofentries < fr->spec->max_entries_int_node))) {
                    rnode_add_rentry(ret->o_nodes[i], e);
                    //we found an o_node with space, then we add the new entry
                    position = ret->o_nodes[i]->nofentries - 1;
                    forb_put_mod_rnode(&fr->base, fr->spec, hash_entry->o_nodes[i], position, rentry_clone(e), height);

#ifdef COLLECT_STATISTICAL_DATA    
                    if (height > 0)
                        _written_int_node_num++;
                    else
                        _written_leaf_node_num++;
                    insert_writes_per_height(height, 1);
#endif

                    inserted = true;
                }
            }
            //if we have not found an O-node with space, we have to create a new O-node
            if (!inserted) {
                hash_entry->k++;
                hash_entry->o_nodes = (int*) lwrealloc(hash_entry->o_nodes, sizeof (int) * hash_entry->k);

                ret->n = hash_entry->k;
                ret->o_nodes = (RNode**) lwrealloc(ret->o_nodes, sizeof (RNode*) * hash_entry->k);
                ret->o_nodes_pages = (int*) lwrealloc(ret->o_nodes_pages, sizeof (int) * hash_entry->k);

                //create a new O-node for the p_node
                hash_entry->o_nodes[hash_entry->k - 1] = rtreesinfo_get_valid_page(fr->info);
                //put this node into the buffer
                forb_create_new_rnode(&fr->base, fr->spec, hash_entry->o_nodes[hash_entry->k - 1], height);
                //put the entry into this node (in the buffer)
                position = 0;
                forb_put_mod_rnode(&fr->base, fr->spec, hash_entry->o_nodes[hash_entry->k - 1], position, 
                        rentry_clone(e), height);

#ifdef COLLECT_STATISTICAL_DATA    
                if (height > 0) {
                    _written_int_node_num++;
                } else {
                    _written_leaf_node_num++;
                }
                insert_writes_per_height(height, 1);
#endif

                ret->o_nodes[hash_entry->k - 1] = rnode_create_empty();
                rnode_add_rentry(ret->o_nodes[hash_entry->k - 1], e);
                ret->o_nodes_pages[hash_entry->k - 1] = hash_entry->o_nodes[hash_entry->k - 1];
            }
            //we have to evaluate if a merge-back operation is needed
            if (hash_entry->tsc >= floor((5 * hash_entry->k - 1) / 2) * (fr->spec->y / fr->spec->x)) {
                int j;
                RNode *oldp;
                FORNodeSet *s;

             //   _DEBUGF(NOTICE, "X value = %f, Y value = %f", fr->spec->x, fr->spec->y);

             //   _DEBUGF(NOTICE, "Performing a merge-back operation - hash_entry->k is equal to %d "
              //          "and hash_entry->tsc equal to %d",
             //           hash_entry->k, hash_entry->tsc);

                oldp = rnode_clone(p);

                rnode_free(p);
                p = rnode_create_empty();

                s = fortree_nodeset_create(1);
                s->o_nodes[0] = rnode_create_empty();

                /*It is important to note that 
                 * we have to remove all the existing entries from the buffer (in the reverse order)
                 * this is done because of the following statement of the FOR-tree original paper:
                 * It is interesting to see that the merge-back operation has an
impact on the flash memory space overhead of FOR-tree. As shown
in Algorithm 1, AddElement merges back the O-nodes of P and pro-
duce a new node P as well as a node set S. P and the nodes in S con-
tain the element e, the old elements in P, and the old elements in
all its O-nodes. If we delete the original O-nodes and create new
nodes during the merge-back operation, FOR-tree will consume
one more flash page than R-tree for each O-node, and in turn intro-
duce large space overheads. Another way is to reuse the existing O-
nodes to store the new nodes in S, since the O-nodes are free for
use after being merged back. To this extent, FOR-tree consumes
the same amount of flash memory space as traditional R-tree, or
even less.*/
                for (i = oldp->nofentries - 1; i >= 0; i--) {
                    forb_put_mod_rnode(&fr->base, fr->spec, p_node, i, NULL, height);
                }
                for (i = 0; i < ret->n; i++) {
                    for (j = ret->o_nodes[i]->nofentries - 1; j >= 0; j--) {
                        forb_put_mod_rnode(&fr->base, fr->spec, ret->o_nodes_pages[i], j, NULL, height);
                    }
                }

#ifdef COLLECT_STATISTICAL_DATA    
                //we compute as written since we will rewrite the entries (see below)
                if (height > 0)
                    _written_int_node_num += ret->n + 1;
                else
                    _written_leaf_node_num += ret->n + 1;
                insert_writes_per_height(height, ret->n + 1);
#endif
                // merge - back operation - we reuse the existing pages of the O-nodes as much as possible
                fortree_mergeback(fr, ret, s, oldp, p, p_node, height);

                //we have to delete the O-nodes that have no entries
                for (i = s->n; i < ret->n; i++) {
                    forb_put_del_rnode(&fr->base, fr->spec, ret->o_nodes_pages[i], height);
                    //we add the removed page as an empty page now
                    rtreesinfo_add_empty_page(fr->info, ret->o_nodes_pages[i]);
#ifdef COLLECT_STATISTICAL_DATA    
                    if (height > 0) {
                        _deleted_int_node_num++;
                    } else {
                        _deleted_leaf_node_num++;
                    }
                    insert_writes_per_height(height, 1);
#endif
                }

                *mb = true;

                //remove these o-nodes from the hash table
                HASH_DEL(ont, hash_entry);

                //free the hash_entry
                lwfree(hash_entry->o_nodes);
                lwfree(hash_entry);

                //free used memory
                rnode_free(oldp);
                fortree_nodeset_destroy(ret);

                ret = s;

                //_DEBUG(NOTICE, "Merge back done");
            } else {
                *mb = false;
            }
        }
    }
    return ret;
}

/*this function returns the p-node to be insert the new entry (input)*/
RNode *fortree_choose_node(FORTree *fr, REntry *input, int h, FORNodeStack *stack, int *chosen_address) {
    RNode *cur_node;
    RNode *n = NULL; //it will be always a p-node
    RNode *p_node = NULL;

    int p_node_add;
    FORNodeSet *s = NULL;

    int tree_height;

    int i, j; //counters for the FOR-tree
    int k; //number of nodes to traverse (p-node + possible o-nodes)
    int entry = 0;
    double enlargement, aux;
    bool is_onode;

    OverflowNodeTable *hash_entry;
    tree_height = fr->info->height;
    HASH_FIND_INT(ont, &fr->info->root_page, hash_entry);
    if (hash_entry != NULL) {
        /*we only increment this value for internal nodes!
         * The reason is that the ChooseLeaft of the FOR-tree original paper 
         *   does not increment this attribute for leaf nodes
         * */
        if (tree_height != 0) {
            hash_entry->tsc++;
        }
        k = hash_entry->k + 1;
    } else {
        k = 1;
    }

    n = rnode_clone(fr->current_node); //because of our stack that stores references
    *chosen_address = fr->info->root_page;
    p_node_add = fr->info->root_page;
    is_onode = false;

    while (true) {
        //yay we found the node N
        if (tree_height == h) {
            if (n == NULL) {
                _DEBUG(ERROR, "Node is null in fortree_choose_node");
            }
            return n;
        }

        /*we store the O-nodes in the stack for each p-node in our path*/
        if (k > 1) {
            s = fortree_nodeset_create(hash_entry->k);
        }

        enlargement = DBL_MAX;
        entry = 0;
        cur_node = rnode_clone(n); //we set the current node as the clone of n
        p_node = rnode_clone(n); //this is the p_node
        //for each node (p-node + its o-nodes)
        for (j = 0; j < k; j++) {
            if (j > 0) {
                rnode_free(cur_node);
                cur_node = forb_retrieve_rnode(&fr->base, hash_entry->o_nodes[j - 1], tree_height);
                s->o_nodes[j - 1] = rnode_clone(cur_node);
                s->o_nodes_pages[j - 1] = hash_entry->o_nodes[j - 1];
#ifdef COLLECT_STATISTICAL_DATA
                if (tree_height != 0) {
                    //we visited one internal node, then we add it
                    _visited_int_node_num++;
                } else {
                    //we visited one leaf node
                    _visited_leaf_node_num++;
                }
                insert_reads_per_height(tree_height, 1);
#endif
            }
            //for each entry of the node
            for (i = 0; i < cur_node->nofentries; i++) {
#ifdef COLLECT_STATISTICAL_DATA
                _processed_entries_num++;
#endif
                aux = bbox_area_of_required_expansion(input->bbox, cur_node->entries[i]->bbox);
                //the entry i is better than the previous one
                if (aux < enlargement) {
                    enlargement = aux; //we update the least enlargement
                    entry = i;

                    if (j > 0) {
                        is_onode = true;
                        *chosen_address = hash_entry->o_nodes[j - 1];
                        rnode_free(n);
                        n = rnode_clone(cur_node);
                    } else {
                        is_onode = false;
                    }
                } else if (DB_IS_EQUAL(aux, enlargement)) {
                    //there is a tie; therefore, we choose the entry of smallest area
                    if (bbox_area(n->entries[i]->bbox) < bbox_area(n->entries[entry]->bbox)) {
                        enlargement = aux;
                        entry = i;

                        if (j > 0) {
                            is_onode = true;
                            *chosen_address = hash_entry->o_nodes[j - 1];
                            rnode_free(n);
                            n = rnode_clone(cur_node);
                        } else {
                            is_onode = false;
                        }
                    }
                }
            }
        }

        //OK we have chosen the better path, 
        //then we put it in our stack to adjust this node after        
        fornode_stack_push(stack, n, *chosen_address, entry, is_onode, p_node, p_node_add, s);
        p_node = NULL;
        s = NULL;
        rnode_free(cur_node);

        *chosen_address = n->entries[entry]->pointer;
        n = forb_retrieve_rnode(&fr->base, n->entries[entry]->pointer, tree_height - 1);

#ifdef COLLECT_STATISTICAL_DATA
        if (tree_height - 1 != 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(tree_height, 1);
#endif

        //we check if we need a next iteration and prepare the corresponding variables
        if ((tree_height - 1) != h) {
            p_node_add = *chosen_address;
            HASH_FIND_INT(ont, &p_node_add, hash_entry);
            if (hash_entry != NULL) {
                //if this node has o-nodes, we increment the tsc value
                hash_entry->tsc++;
                k = hash_entry->k + 1;
            } else {
                k = 1;
            }
        }

        tree_height--;
    }
    _DEBUG(ERROR, "Oops, no node was chosen in choose_node.");
    return NULL;
}

FORNodeSet *fortree_adjust_tree(FORTree *fr, RNode *l, FORNodeSet *s, bool *mb, int l_height, FORNodeStack *stack) {
    BBox *bbox; //used to adjust an entry

    /*the page number of the parent node
     * the parent node is stored in the fr->current_node */
    int parent_add;
    /* the page number of the p-node of the parent node
     * the p-node is stored, which can be the same node of the fr->current_node*/
    int p_node_add;
    RNode *p_node = NULL;

    //the index of the entry of the parent node (which is the fr->current_node)
    int entry;
    int h = l_height; //the current height of the tree
    bool n_is_onode; //it says if n is a o-node or not
    RNode *n; //an RNode used to help in the adjustment

    //this nodeset stores the O-nodes of the p_node 
    FORNodeSet *ss;
    //this nodeset is an auxiliary pointer
    FORNodeSet *tmp = NULL;
    //it stores the o-nodes of p_node after a merge back operation
    FORNodeSet *onodes_after_mb = NULL;

    bool adjusting = true;

    n = rnode_clone(l);
    n_is_onode = false;
    p_node = rnode_clone(n);
    ss = fortree_nodeset_clone(s);
    //current_node will be the parent of n
    rnode_free(fr->current_node);
    fr->current_node = NULL;

    while (h != fr->info->height) {
        /*if we are not adjusting the tree anymore because it is not more necessary
            we stop!*/
        if (!adjusting)
            break;

        /* first step: calculate the bbox to adjust the bbox of the parent's entry */

        /*if there is a merge back operation */
        if (*mb) {
            //this calculation considers the p-node N plus its O-nodes created after a merge back operation
            if (onodes_after_mb != NULL && onodes_after_mb->n > 0) {
                bbox = fortree_union_allnodes(n, onodes_after_mb);
            } else { //this calculation considers the p-node N only                
                bbox = rnode_compute_bbox(n);
            }
        } else {
            /*otherwise, we compute the bbox of the p-node plus its o-nodes */
            if (n_is_onode) { //n is a O-node
                bbox = fortree_union_allnodes(p_node, ss);
            } else { //n is a P-node here
                bbox = fortree_union_allnodes(n, ss);
            }
        }
        //we free the possible p_node and its o-nodes since the fornode_stack_pop will update this info
        rnode_free(p_node);
        fortree_nodeset_destroy(tmp);
        p_node = NULL;
        tmp = NULL;
        //the current_node is the parent of N (which can be an o-node)
        fr->current_node = fornode_stack_pop(stack, &parent_add, &entry, &n_is_onode, &p_node, &p_node_add, &tmp);

        //if there is no a previous merge back operation
        if (!(*mb)) {
            //we check if it is necessary to modify the BBOX of this parent
            if (!bbox_check_predicate(bbox, fr->current_node->entries[entry]->bbox, EQUAL)) {
                memcpy(fr->current_node->entries[entry]->bbox, bbox, sizeof (BBox));

              //  _DEBUG(NOTICE, "ajustou a entrada do pai");

                forb_put_mod_rnode(&fr->base, fr->spec, parent_add, entry, 
                        rentry_clone(fr->current_node->entries[entry]), h + 1);

#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num++;
                insert_writes_per_height(h + 1, 1);
#endif
                adjusting = true;

                //we move up to next level
                rnode_free(n);
                n = fr->current_node;
                fr->current_node = NULL;
                fortree_nodeset_destroy(ss);
                ss = tmp;
                tmp = NULL;
            } else {
                //_DEBUG(NOTICE, "Nao precisa ajustar mais nada");
                adjusting = false;
            }
        } else {
            /*in this case, we performed a previous merge back operation
             therefore, we have to add the new p-nodes as entries in the parent node*/
            int i;
            REntry *rentry = NULL;
            FORNodeSet *ss_for_mb = NULL;
            bool occured_mb = false;

            memcpy(fr->current_node->entries[entry]->bbox, bbox, sizeof (BBox));
            forb_put_mod_rnode(&fr->base, fr->spec, parent_add, entry, 
                    rentry_clone(fr->current_node->entries[entry]), h + 1);

            //_DEBUG(NOTICE, "The parent node before the insertion of new entries from nodeset s");
            //rnode_print(fr->current_node, parent_add);

#ifdef COLLECT_STATISTICAL_DATA
            _written_int_node_num++;
            insert_writes_per_height(h + 1, 1);
#endif                 
            fortree_nodeset_destroy(onodes_after_mb);
            for (i = 0; i < ss->n; i++) {
                fortree_nodeset_destroy(tmp);
                //    _DEBUGF(NOTICE, "Inserting the entry of the ss %d", i);

                rentry = rentry_create(ss->o_nodes_pages[i], rnode_compute_bbox(ss->o_nodes[i]));
                if (n_is_onode) {
                    //we consider the p_node
                    tmp = fortree_add_element(fr, h, p_node_add, p_node, rentry, mb);
                } else {
                    //we consider the current_node
                    tmp = fortree_add_element(fr, h, parent_add, fr->current_node, rentry, mb);
                }
                rentry = NULL;
                /* we check if the previous operation performed a merge back operation
                 * if this is the case, 
                 *   we store a reference for the nodeset S of this merge back
                 * aqui tem que ser somente do ultimo add element, sen a gente perde referencia dos que foram inseridos primeiro
                 * dessa forma, deixa para "habilitar' o merge back somente na ultima insercao
                 */
                if (*mb) {
                    //_DEBUG(NOTICE, "Guardou o nodeset s resultante desse merge back");
                    occured_mb = true;
                    ss_for_mb = fortree_nodeset_clone(tmp);
                }
            }
            fortree_nodeset_destroy(ss);
            //(NOTICE, "Limpou ss");

            //we inserted the entries in the current_node and then we have to update p_node
            if (!n_is_onode) {
                rnode_free(p_node);
                p_node = rnode_clone(fr->current_node);
            }

            /*
            if (*mb) {
                _DEBUG(NOTICE, "Ocorreu merge back");
            } else {
                _DEBUG(NOTICE, "Nao ocorreu merge back");
            }*/

            /*if a merge back was performed before the last insertion*/
            if (occured_mb) {
                //_DEBUG(NOTICE, "Ocorreu um merge back no pai e devemos propagar o nodeset s");
                //we have to insert these nodes in the parent p-node
                ss = ss_for_mb;
                ss_for_mb = NULL;
                //_DEBUGF(NOTICE, "Precisamos adicionar %d entradas no parent node", ss->n);
                //we have to also consider the following o-nodes generated after the merge back operation
                onodes_after_mb = tmp;
                tmp = NULL;
                *mb = true;
            } else {
                //we have a set of o-nodes to consider in the calculation of the bbox
                //there is no nodeset to be propagated
                ss = tmp;
                tmp = NULL;
                onodes_after_mb = NULL;
                *mb = false;
            }

            //we move up to next level
            rnode_free(n);
            n = fr->current_node;

//            _DEBUG(NOTICE, "The parent node after the insertion of new entries from nodeset s");
           // rnode_print(fr->current_node, parent_add);

            fr->current_node = NULL;
        }
        lwfree(bbox);

        h++;
    }

    //_DEBUG(NOTICE, "Apagando o tmp");
    if (tmp != NULL) {
        //_DEBUGF(NOTICE, "O tamanho de tmp eh %d", tmp->n);
    }
    fortree_nodeset_destroy(tmp);
    tmp = NULL;

    /*we stopped to adjusted the tree, we set the current_node as the root p-node here*/
    if (!adjusting) {
        if (stack->size > 0) {
            //(NOTICE, "Parou de ajustar no meio do caminho");
            rnode_free(fr->current_node);
            while (stack->size > 0) {
                fr->current_node = fornode_stack_pop(stack, &parent_add, &entry, &n_is_onode, &p_node, &p_node_add, &tmp);
                if (n_is_onode) {
                    rnode_free(fr->current_node);
                    fr->current_node = p_node;
                } else {
                    rnode_free(p_node);
                }
                fortree_nodeset_destroy(tmp);
            }
            rnode_free(n);
        } else {
            //(NOTICE, "Parou de ajustar mas chegou ateh o root");
            if (n_is_onode) {
                rnode_free(n);
                fr->current_node = p_node;
            } else {
                rnode_free(p_node);
            }
        }
    } else {
        //(NOTICE, "Ajustou ate o fim, o root node eh o pnode");
        rnode_free(n);
        fr->current_node = p_node;
    }
    //(NOTICE, "No raiz eh :");
   // rnode_print(fr->current_node, fr->info->root_page);
    //if we have a merge back operation to be applied in the root node, 
    //then we have to return our ss
    if (ss != NULL && ss->n > 0 && (*mb)) {
        return ss;
    } else {
        fortree_nodeset_destroy(ss);
    }
    return NULL;

}

/*inserting algorithm of the FOR-tree*/
void fortree_insert_entry(FORTree *fr, REntry *input, int input_height) {
    RNode *chosen_node; //the node in which was chosen to insert the input
    int chosen_address; //page number of the chosen_node, which is a P-Node
    FORNodeSet *s; //the result of the add an element in the chosen_node
    FORNodeSet *ss; //the result of the adjust tree
    bool mb; //occurred a merge back operation?
    FORNodeStack *stack; //the stack for fornodes in order to be used in the adjust tree

    stack = fornode_stack_init();

    /*1: Invoke ChooseLeaf(R, e) to select a leaf node L to place
elemente in the FOR-tree R;*/
    chosen_node = fortree_choose_node(fr, input, input_height, stack, &chosen_address);

    //(NOTICE, "fortree_choose_node done - calling fortree_add_element");

    /*2: Invoke AddElement(L, e) to insert the element e into the leaf
node L;*/
    s = fortree_add_element(fr, input_height, chosen_address, chosen_node, input, &mb);

    //(NOTICE, "fortree_add_element done - calling fortree_adjust_tree");

    /*3: Invoke AdjustTree(L) to adjust FOR-tree by modifying MBRs
and propagating node merges;*/
    ss = fortree_adjust_tree(fr, chosen_node, s, &mb, input_height, stack);

    //(NOTICE, "fortree_adjust_tree done");

    fortree_nodeset_destroy(s);

    /*4: If the propagation caused O-nodes of the root R to merge
back and produced a node set S Then*/
    if (ss != NULL && ss->n > 0 && mb) {
        //then we have to create a new root with n entries
        RNode *new_root = rnode_create_empty();
        int new_root_add;
        int i;

        REntry *entry;
        OverflowNodeTable *hash_entry;

        //(NOTICE, "growing up the tree");

        //we allocate one more page for the new root
        new_root_add = rtreesinfo_get_valid_page(fr->info);
        fr->info->height++;
        
        storage_update_tree_height(&fr->base, fr->info->height);

#ifdef COLLECT_STATISTICAL_DATA
        _written_int_node_num++;
        insert_writes_per_height(fr->info->height, 1);
#endif

        //put this node into the buffer
        forb_create_new_rnode(&fr->base, fr->spec, new_root_add, fr->info->height);

        /*5: create a new root whose children are R and nodes in S;
6: End If*/

        /*adding firstly the entry R*/
        HASH_FIND_INT(ont, &(fr->info->root_page), hash_entry);
        if (hash_entry != NULL) {
            s = NULL;
            s = fortree_nodeset_create(hash_entry->k);
            for (i = 0; i < hash_entry->k; i++) {
            //    _DEBUGF(NOTICE, "Computou o o-node %d do antigo root", s->o_nodes_pages[i]);
                s->o_nodes[i] = forb_retrieve_rnode(&fr->base, hash_entry->o_nodes[i], fr->info->height - 1);
                s->o_nodes_pages[i] = hash_entry->o_nodes[i];
            }
            entry = rentry_create(fr->info->root_page, fortree_union_allnodes(fr->current_node, s));
            fortree_nodeset_destroy(s);
            //(NOTICE, "O-Nodes do antigo root node computados");
        } else {
            entry = rentry_create(fr->info->root_page, rnode_compute_bbox(fr->current_node));
            //(NOTICE, "O antigo root node era um p-node puro");
        }
        fortree_add_element(fr, fr->info->height, new_root_add, new_root, entry, &mb);
        entry = NULL;

        /* we then add the elements in SS (from the previous merge back operation)
         *  into the root node*/
        for (i = 0; i < ss->n; i++) {
         //   _DEBUGF(NOTICE, "Adding the entry %d in the new root node", ss->o_nodes_pages[i]);

            entry = rentry_create(ss->o_nodes_pages[i], rnode_compute_bbox(ss->o_nodes[i]));
            fortree_add_element(fr, fr->info->height, new_root_add, new_root, entry, &mb);
            entry = NULL;
        }

        //we update the root page
        fr->info->root_page = new_root_add;

        rnode_free(fr->current_node);
        fr->current_node = new_root;

        fortree_nodeset_destroy(ss);

        //(NOTICE, "done");
    }
    rnode_free(chosen_node);
    fornode_stack_destroy(stack);

    //(NOTICE, "element inserted");
}

/*the chosen leaf will return a ChooseLeaf object (see above)*/
ChooseLeaf *fortree_choose_leaf(FORTree *fr, int p_node_add, REntry *to_remove, int height, FORNodeStack *stack, ChooseLeaf *cl) {
    RNode *cur_node;
    RNode *p_node;
    int child_add;
    FORNodeSet *s = NULL;
    int i, k, j;

    /* to get all o_nodes of the node_page */
    OverflowNodeTable *hash_entry;

    /*for backtracking purposes */
    cur_node = rnode_clone(fr->current_node);
    p_node = rnode_clone(cur_node);

    //(NOTICE, "THE P-NODE");

   // rnode_print(p_node, p_node_add);

    HASH_FIND_INT(ont, &p_node_add, hash_entry);
    if (hash_entry != NULL) {
        /*we only increment this value for internal nodes!
         * The reason is that the ChooseLeaft of the FOR-tree original paper 
         *   does not increment this attribute for leaf nodes
         * */
        if (height != 0) {
            hash_entry->tsc++;
        }
        k = hash_entry->k + 1;
    } else {
        k = 1;
    }

    /*we store the O-nodes in the stack for each p-node in our path*/
    if (k > 1) {
        //(NOTICE, "ITS O-NODES");
        s = fortree_nodeset_create(hash_entry->k);
        for (j = 0; j < hash_entry->k; j++) {
            s->o_nodes[j] = forb_retrieve_rnode(&fr->base, hash_entry->o_nodes[j], height);
            s->o_nodes_pages[j] = hash_entry->o_nodes[j];

          //  rnode_print(s->o_nodes[j], s->o_nodes_pages[j]);
        }
    }

    if (height != 0) {
        for (j = 0; j < k; j++) {
            if (j > 0) {
                rnode_free(fr->current_node);
                fr->current_node = rnode_clone(s->o_nodes[j - 1]);

#ifdef COLLECT_STATISTICAL_DATA                
                _visited_int_node_num++;
                insert_reads_per_height(height, 1);
#endif
                rnode_free(cur_node);
                cur_node = rnode_clone(fr->current_node);
            }

            for (i = 0; i < fr->current_node->nofentries; i++) {

#ifdef COLLECT_STATISTICAL_DATA
                _processed_entries_num++;
#endif

                if (bbox_check_predicate(to_remove->bbox, fr->current_node->entries[i]->bbox, INSIDE_OR_COVEREDBY)) {
                    //we get the node in which the entry points to
                    child_add = fr->current_node->entries[i]->pointer;

                    if (j > 0) {
                        fornode_stack_push(stack, rnode_clone(fr->current_node), s->o_nodes_pages[j - 1], i,
                                true, rnode_clone(p_node), p_node_add, fortree_nodeset_clone(s));
                    } else {
                        fornode_stack_push(stack, rnode_clone(fr->current_node), p_node_add, i,
                                false, rnode_clone(fr->current_node), p_node_add, fortree_nodeset_clone(s));
                    }

                    rnode_free(fr->current_node);
                    fr->current_node = forb_retrieve_rnode(&fr->base, child_add, height - 1);

#ifdef COLLECT_STATISTICAL_DATA
                    if (height - 1 != 0) {
                        //we visited one internal node, then we add it
                        _visited_int_node_num++;
                    } else {
                        //we visited one leaf node
                        _visited_leaf_node_num++;
                    }
                    insert_reads_per_height(height - 1, 1);
#endif

                    cl = fortree_choose_leaf(fr, child_add, to_remove, height - 1, stack, cl);

                    if (cl->entry_chosen_node == -1) {
                        //(NOTICE, "Esse caminho nao eh bom");
                        fornode_stack_pop_without_return(stack);
                        /*    after to traverse this child, we need to back
                         * the reference of the current_node for the original one*/
                        rnode_copy(fr->current_node, cur_node);
                        //(NOTICE, "Vamos voltar para o no antigo: ");
                      //  rnode_print(fr->current_node, p_node_add);
                    } else {
                        rnode_free(cur_node);
                        return cl;
                    }
                }
            }
        }
    } else {
        for (j = 0; j < k; j++) {
            if (j > 0) {
                rnode_free(fr->current_node);
                fr->current_node = rnode_clone(s->o_nodes[j - 1]);
#ifdef COLLECT_STATISTICAL_DATA                
                _visited_leaf_node_num++;
                insert_reads_per_height(height, 1);
#endif                
            }
            for (i = 0; i < fr->current_node->nofentries; i++) {
#ifdef COLLECT_STATISTICAL_DATA
                _processed_entries_num++;
#endif
                if (fr->current_node->entries[i]->pointer == to_remove->pointer) {
                    cl->chosen_node = rnode_clone(fr->current_node);
                    if (j > 0)
                        cl->chosen_node_add = hash_entry->o_nodes[j - 1];
                    else
                        cl->chosen_node_add = p_node_add;
                    cl->entry_chosen_node = i;

                    cl->s = fortree_nodeset_clone(s);

                    cl->p_node = rnode_clone(p_node);
                    cl->p_node_add = p_node_add;
                    rnode_free(cur_node);
                    rnode_free(p_node);
                    fortree_nodeset_destroy(s);
                    return cl;
                }
            }
        }
        //if (cl->entry_chosen_node == -1) {
            //(NOTICE, "Nao achou nada nesse leaf node");
        //}
    }
    rnode_free(cur_node);
    rnode_free(p_node);
    fortree_nodeset_destroy(s);
    return cl;
}

void fortree_condense_tree(FORTree *fr, ChooseLeaf *cl, FORNodeStack *stack) {
    int cur_height;
    int tree_height;

    BBox *bbox;

    RNode *n;
    int n_add;
    RNode *p_node_of_n;
    int p_node_of_n_add;
    bool is_onode;
    FORNodeSet *s_of_n = NULL;

    int parent_add;
    int parent_entry, i;
    bool parent_is_onode;
    RNode *parent_p_node = NULL;
    int parent_p_node_add;
    FORNodeSet *parent_s = NULL;

    FORNodeStack *removed_nodes;

    bool adjusting = true;

   // rnode_print(cl->chosen_node, cl->chosen_node_add);
   // rnode_print(cl->p_node, cl->p_node_add);

    n = rnode_clone(cl->chosen_node);
    n_add = cl->chosen_node_add;
    p_node_of_n = rnode_clone(cl->p_node);
    p_node_of_n_add = cl->p_node_add;

    /*
    if (cl->s != NULL) {
        _DEBUGF(NOTICE, "Fazendo o clone de s que tem %d elementos", cl->s->n);
    } else {
        _DEBUG(NOTICE, "cl->s eh null");
    }*/

    s_of_n = fortree_nodeset_clone(cl->s);

    if (cl->chosen_node_add == cl->p_node_add)
        is_onode = false;
    else
        is_onode = true;

    rnode_free(fr->current_node);
    fr->current_node = NULL; //this will be the parent of n
    parent_entry = 0;
    cur_height = 0; //the current level
    tree_height = fr->info->height; //the height of the tree (height == 0 means leaf node)

    removed_nodes = fornode_stack_init();

    while (cur_height != tree_height) {
        if (!adjusting)
            break;

        fr->current_node = fornode_stack_pop(stack, &parent_add, &parent_entry,
                &parent_is_onode, &parent_p_node, &parent_p_node_add, &parent_s);

        //(NOTICE, "Pegou o pai");
      //  rnode_print(fr->current_node, parent_add);

        /*if this is an O-Node, we check if this node has no entries. We do not consider the min entries attribute here 
         * since an O-Node can have lesser than this minimal value (this is allowed by the FOR-tree).
         The other situation is that the P-Node has not the minimum occupancy.*/
        if ((is_onode && n->nofentries == 0) ||
                (!is_onode && ((cur_height == 0 && n->nofentries < fr->spec->min_entries_leaf_node) ||
                (cur_height != 0 && n->nofentries < fr->spec->min_entries_int_node)))) {

            //here we have to perform a merge back operation for the n and replace its o-nodes by the new ones
            //it handles the two following cases: n is an o-node, or n is a p-node with o-nodes!
            if (is_onode || (!is_onode && (s_of_n != NULL || s_of_n->n > 0))) {
                OverflowNodeTable *hash_entry;
                int j;
                RNode *oldp;
                FORNodeSet *new_s;

                //(NOTICE, "precisamos fazer o merge back aqui por causa da remocao");

                oldp = rnode_clone(p_node_of_n);

                //we refresh the P-Node of n
                rnode_free(p_node_of_n);
                p_node_of_n = rnode_create_empty();

                //we refresh its O-Nodes as a nodeset s
                new_s = fortree_nodeset_create(1);
                new_s->o_nodes[0] = rnode_create_empty();

                //we have to remove all the existing entries from the buffer
                for (i = oldp->nofentries - 1; i >= 0; i--)
                    forb_put_mod_rnode(&fr->base, fr->spec, p_node_of_n_add, i, NULL, cur_height);
                for (i = s_of_n->n - 1; i >= 0; i--) {
                    //we have to remove all entries of all valid o-nodes
                    //n_add here can be an empty o-node, and thus we can't remove entries
                    if (s_of_n->o_nodes_pages[i] != n_add) {
                        for (j = s_of_n->o_nodes[i]->nofentries - 1; j >= 0; j--)
                            forb_put_mod_rnode(&fr->base, fr->spec, s_of_n->o_nodes_pages[i], j, NULL, cur_height);
                    } else {
                        //but, n here is an empty o-node, then we have to remove its all entries from the nodeset S
                        for (j = s_of_n->o_nodes[i]->nofentries - 1; j >= 0; j--)
                            rnode_remove_rentry(s_of_n->o_nodes[i], j);
                    }
                }


#ifdef COLLECT_STATISTICAL_DATA    
                //we compute as written since we will rewrite the entries (see below)
                if (cur_height > 0)
                    _written_int_node_num += s_of_n->n + 1;
                else
                    _written_leaf_node_num += s_of_n->n + 1;
                insert_writes_per_height(cur_height, s_of_n->n + 1);
#endif
                // merge - back operation - we reuse the existing pages of the O-nodes as much as possible
                fortree_mergeback(fr, s_of_n, new_s, oldp, p_node_of_n, p_node_of_n_add, cur_height);

                //_DEBUGF(NOTICE, "Tamanho da lista de o-nodes antigo %d e depois do merge back %d",
                //        s_of_n->n, new_s->n);


                //we have to delete the O-nodes that have no entries
                for (i = new_s->n; i < s_of_n->n; i++) {
                    //_DEBUGF(NOTICE, "APAGOU O NO %d", s_of_n->o_nodes_pages[i]);
                    forb_put_del_rnode(&fr->base, fr->spec, s_of_n->o_nodes_pages[i], cur_height);

                    //we add the removed page as an empty page now
                    rtreesinfo_add_empty_page(fr->info, s_of_n->o_nodes_pages[i]);

#ifdef COLLECT_STATISTICAL_DATA    
                    if (cur_height > 0) {
                        _deleted_int_node_num++;
                    } else {
                        _deleted_leaf_node_num++;
                    }
                    insert_writes_per_height(cur_height, 1);
#endif
                }

                HASH_FIND_INT(ont, &p_node_of_n_add, hash_entry);

                //remove these o-nodes from the hash table
                HASH_DEL(ont, hash_entry);

                //free the hash_entry
                lwfree(hash_entry->o_nodes);
                lwfree(hash_entry);

                //we only will insert new data into the hash table if O-nodes were created
                if (new_s->n > 0) {
                    //(NOTICE, "Atualizando a hash table dos o-nodes")
                    HASH_FIND_INT(ont, &p_node_of_n_add, hash_entry);
                    if (hash_entry == NULL) {
                        //then create a new hash_entry
                        hash_entry = (OverflowNodeTable*) lwalloc(sizeof (OverflowNodeTable));
                        hash_entry->k = new_s->n;
                        hash_entry->tsc = 0;
                        hash_entry->node_id = p_node_of_n_add;
                        hash_entry->o_nodes = (int*) lwalloc(sizeof (int) * new_s->n);

                        //we add back the merged o-nodes into the hash
                        memcpy(hash_entry->o_nodes, new_s->o_nodes_pages, new_s->n * sizeof (int));
                        HASH_ADD_INT(ont, node_id, hash_entry);
                    } else {
                        _DEBUG(ERROR, "The hashentry was not removed from the hash table!");
                    }
                }

                /* now we have to update the MBR of the parent entry if necessary*/
                if (new_s->n == 0) { //we calculate the MBR of the p-node
                    bbox = rnode_compute_bbox(p_node_of_n);
                } else { //we calculate the MBR of N and its O-Nodes
                    bbox = fortree_union_allnodes(p_node_of_n, new_s);
                }

                if (!bbox_check_predicate(bbox, fr->current_node->entries[parent_entry]->bbox, EQUAL)) {
                    memcpy(fr->current_node->entries[parent_entry]->bbox, bbox, sizeof (BBox));
                    forb_put_mod_rnode(&fr->base, fr->spec, parent_add, parent_entry,
                            rentry_create(fr->current_node->entries[parent_entry]->pointer, bbox), cur_height + 1);
                    bbox = NULL;

#ifdef COLLECT_STATISTICAL_DATA
                    _written_int_node_num++;
                    insert_writes_per_height(cur_height + 1, 1);
#endif
                    adjusting = true;

                    //(NOTICE, "Ataulizou a entrada do no pai");
                } else {
                    adjusting = false;
                    lwfree(bbox);
                }

                //free used memory
                rnode_free(oldp);
                fortree_nodeset_destroy(new_s);

                //we destroy the N
                rnode_free(n);

                //we destroy the P-Node of n
                rnode_free(p_node_of_n);

                //we update the nodeset S of N
                fortree_nodeset_destroy(s_of_n);
            } else {
                //this node is a p-node and does not have o-nodes, we must remove it     
                forb_put_del_rnode(&fr->base, fr->spec, n_add, cur_height);
                //we add the removed page as an empty page now
                rtreesinfo_add_empty_page(fr->info, n_add);

                fornode_stack_push(removed_nodes, n, cur_height, n_add, is_onode, p_node_of_n, p_node_of_n_add, s_of_n);
                n = NULL;
                p_node_of_n = NULL;
                s_of_n = NULL;
                //(NOTICE, "P-node com underflow");

#ifdef COLLECT_STATISTICAL_DATA
                if (cur_height != 0) {
                    //we removed an internal node, then we add it
                    _deleted_int_node_num++;
                } else {
                    //we removed a leaf node
                    _deleted_leaf_node_num++;
                }
                insert_writes_per_height(cur_height, 1);
#endif

                rnode_remove_rentry(fr->current_node, parent_entry);
                forb_put_mod_rnode(&fr->base, fr->spec, parent_add, parent_entry, NULL, cur_height + 1);
#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num++;
                insert_writes_per_height(cur_height + 1, 1);
#endif
            }
        } else {
            //(NOTICE, "we need to check if an update is needed in its parent node");

           // rnode_print(p_node_of_n, p_node_of_n_add);

            /* now we have to update the MBR of the parent entry if necessary*/
            bbox = fortree_union_allnodes(p_node_of_n, s_of_n);

            if (!bbox_check_predicate(bbox, fr->current_node->entries[parent_entry]->bbox, EQUAL)) {
                //(NOTICE, "Precisamos ajustar");
                memcpy(fr->current_node->entries[parent_entry]->bbox, bbox, sizeof (BBox));
                forb_put_mod_rnode(&fr->base, fr->spec, parent_add, parent_entry,
                        rentry_create(fr->current_node->entries[parent_entry]->pointer, bbox), cur_height + 1);
                bbox = NULL;

                //(NOTICE, "Precisou modificar o bbox do pai");

#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num++;
                insert_writes_per_height(cur_height + 1, 1);
#endif
                adjusting = true;
            } else {
                //(NOTICE, "Nao eh preciso ajustar");
                adjusting = false;
                lwfree(bbox);
            }

            //we destroy the N
            rnode_free(n);

            //we destroy the P-Node of n
            rnode_free(p_node_of_n);

            //we update the nodeset S of N
            fortree_nodeset_destroy(s_of_n);

            //(NOTICE, "Limpou memoria");
        }

        n = fr->current_node;
        fr->current_node = NULL;
        n_add = parent_add;

        p_node_of_n = parent_p_node;
        parent_p_node = NULL;
        p_node_of_n_add = parent_p_node_add;

        s_of_n = parent_s;
        parent_s = NULL;
        //and its info
        is_onode = parent_is_onode;

        cur_height++; //we up one more level until root
    }

    // we stopped to adjusted the tree, we set the current_node as the root node here
    if (!adjusting) {
        //(NOTICE, "nao precisou ajustar a arvore toda");
        if (stack->size > 0) {
            while (stack->size > 0) {
                //(NOTICE, "Subiu um nivel");

                rnode_free(fr->current_node);
                rnode_free(parent_p_node);
                fortree_nodeset_destroy(parent_s);
                fr->current_node = fornode_stack_pop(stack, &parent_add, &parent_entry,
                        &parent_is_onode, &parent_p_node, &parent_p_node_add, &parent_s);
            }
            if (parent_is_onode) {
                //(NOTICE, "O root era um o-node, entao vamos pegar seu p-node");
                rnode_free(fr->current_node);
                fr->current_node = rnode_clone(parent_p_node);
            }
        } else {
            //we update the root node
            if (is_onode)
                fr->current_node = rnode_clone(p_node_of_n);
            else
                fr->current_node = rnode_clone(n);
        }

        //(NOTICE, "ok ");
    } else {
        //we update the root node
        if (is_onode)
            fr->current_node = rnode_clone(p_node_of_n);
        else
            fr->current_node = rnode_clone(n);
    }
    rnode_free(n);
    rnode_free(parent_p_node);
    fortree_nodeset_destroy(parent_s);
    rnode_free(p_node_of_n);
    fortree_nodeset_destroy(s_of_n);
    //(NOTICE, "Limpou memoria");

    parent_p_node = NULL;
    parent_s = NULL;
    n = NULL;

    /*reinsertion of the removed nodes! */
    while (removed_nodes->size > 0) {
        n = fornode_stack_pop(stack, &parent_add, &cur_height,
                &parent_is_onode, &parent_p_node, &parent_p_node_add, &parent_s);
        for (i = 0; i < n->nofentries; i++) {
            fortree_insert_entry(fr, n->entries[i], cur_height);
        }
        rnode_free(n);
        rnode_free(parent_p_node);
        fortree_nodeset_destroy(parent_s);

        parent_p_node = NULL;
        parent_s = NULL;
        n = NULL;
    }

    fornode_stack_destroy(removed_nodes);
}

bool fortree_remove_entry(FORTree *fr, REntry * to_remove) {
    ChooseLeaf *chosen_entry;
    FORNodeStack *stack;

    stack = fornode_stack_init();
    chosen_entry = (ChooseLeaf *) lwalloc(sizeof (ChooseLeaf));
    chosen_entry->chosen_node = NULL;
    chosen_entry->chosen_node_add = -1;
    chosen_entry->entry_chosen_node = -1;
    chosen_entry->p_node = NULL;
    chosen_entry->p_node_add = -1;
    chosen_entry->s = NULL;

    //(NOTICE, "Called choose_leaf");

    chosen_entry = fortree_choose_leaf(fr, fr->info->root_page, to_remove, fr->info->height, stack, chosen_entry);

    //(NOTICE, "Leaf chosen");

    if (chosen_entry->entry_chosen_node != -1 && fr->current_node != NULL) {
        rnode_remove_rentry(chosen_entry->chosen_node, chosen_entry->entry_chosen_node);
        forb_put_mod_rnode(&fr->base, fr->spec, chosen_entry->chosen_node_add, 
                chosen_entry->entry_chosen_node, NULL, 0);
        if (chosen_entry->chosen_node_add == chosen_entry->p_node_add) {
            rnode_remove_rentry(chosen_entry->p_node, chosen_entry->entry_chosen_node);
        }
        //(NOTICE, "Entry removed");
#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif

        fortree_condense_tree(fr, chosen_entry, stack);
        //(NOTICE, "condense tree done");
    }

    //(NOTICE, "Vamos mostar o current node");

    //rnode_print(fr->current_node, fr->info->root_page);

    if (fr->current_node->nofentries == 1 && fr->info->height > 0) {
        int p;
        RNode *new_root;

        //(NOTICE, "Tem que cortar a arvore");

        p = fr->current_node->entries[0]->pointer;

        forb_put_del_rnode(&fr->base, fr->spec, fr->info->root_page, fr->info->height);
        //we add the removed page as an empty page now
        rtreesinfo_add_empty_page(fr->info, fr->info->root_page);

#ifdef COLLECT_STATISTICAL_DATA
        _deleted_int_node_num++;
        insert_writes_per_height(fr->info->height, 1);
#endif

        fr->info->root_page = p;

        new_root = forb_retrieve_rnode(&fr->base, p, fr->info->height - 1);

#ifdef COLLECT_STATISTICAL_DATA
        if (fr->info->height > 1) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
#endif

        rnode_free(fr->current_node);
        fr->current_node = new_root;
        fr->info->height--;
        
        storage_update_tree_height(&fr->base, fr->info->height);
    }

    //(NOTICE, "Limpando stack");
    fornode_stack_destroy(stack);
    //(NOTICE, "Feito");

    if (chosen_entry->entry_chosen_node != -1) {
        rnode_free(chosen_entry->chosen_node);
        rnode_free(chosen_entry->p_node);
        fortree_nodeset_destroy(chosen_entry->s);
        lwfree(chosen_entry);
        //(NOTICE, "memory cleaned");
        return true;
    } else {
        return false;
    }
}

SpatialIndexResult * fortree_search(FORTree *fr, const BBox *query, uint8_t predicate) {
    SpatialIndexResult *sir = spatial_index_result_create();
    /* current node here MUST be equal to the root node */
    if (fr->current_node != NULL) {
        sir = fortree_recursive_search(fr, fr->info->root_page, query, predicate, fr->info->height, sir);
    }
    return sir;
}

int fortree_get_nof_onodes(int n_page) {
    OverflowNodeTable *hash_entry;
    HASH_FIND_INT(ont, &n_page, hash_entry);
    if (hash_entry != NULL) {
        return hash_entry->k;
    }
    return 0;
}

int fortree_get_onode(int n_page, int index) {
    OverflowNodeTable *hash_entry;
    HASH_FIND_INT(ont, &n_page, hash_entry);
    if (hash_entry != NULL) {
        return hash_entry->o_nodes[index];
    }
    return -1;
}

/*********************************
 * functions in order to make FORTree a standard SpatialIndex (see spatial_index.h)
 *********************************/

static uint8_t fortree_get_type(const SpatialIndex * si) {
    FORTree *fr = (void *) si;
    return fr->type;
}

/*original insert such as described in FORTree paper (it does not apply split algorithms!)*/
static bool fortree_insert(SpatialIndex *si, int pointer, const LWGEOM * geom) {
    BBox *bbox = (BBox*) lwalloc(sizeof (BBox));
    FORTree *fr = (void *) si;
    REntry *input;

    gbox_to_bbox(geom->bbox, bbox);

    input = rentry_create(pointer, bbox);

    fortree_insert_entry(fr, input, 0);

    return true;
}

static bool fortree_remove(SpatialIndex *si, int pointer, const LWGEOM * geom) {
    BBox *bbox = (BBox*) lwalloc(sizeof (BBox));
    FORTree *fr = (void *) si;
    REntry *rem;
    bool ret;

    gbox_to_bbox(geom->bbox, bbox);
    rem = rentry_create(pointer, bbox);

    ret = fortree_remove_entry(fr, rem);

    lwfree(rem->bbox);
    lwfree(rem);

    return ret;
}

static bool fortree_update(SpatialIndex *si, int oldpointer, const LWGEOM *oldgeom, int newpointer, const LWGEOM * newgeom) {
    bool r, i = false;
    //TO-DO improve the return value of this.. i.e., it can return an error identifier.
    r = fortree_remove(si, oldpointer, oldgeom);
    if (r)
        i = fortree_insert(si, newpointer, newgeom);
    return r && i;
}

static SpatialIndexResult * fortree_search_ss(SpatialIndex *si, const LWGEOM *search_object, uint8_t predicate) {
    SpatialIndexResult *sir;
    BBox *search = (BBox*) lwalloc(sizeof (BBox));
    FORTree *fr = (void *) si;

    gbox_to_bbox(search_object->bbox, search);

    sir = fortree_search(fr, search, predicate);

    lwfree(search);
    return sir;
}

static bool fortree_header_writer(SpatialIndex *si, const char *file) {
    festival_header_writer(file, FORTREE_TYPE, si);
    return true;
}

static void fortree_destroy(SpatialIndex * si) {
    FORTree *fortree = (void *) si;

    rnode_free(fortree->current_node);
    lwfree(fortree->spec);
    rtreesinfo_free(fortree->info);

    generic_parameters_free(fortree->base.gp);
    source_free(fortree->base.src);
    lwfree(fortree->base.index_file);

    lwfree(fortree);
}

/* return a new FOR-tree index, the specific parameters must be specified later,
 we get also the FORTreespecification*/
SpatialIndex * fortree_empty_create(char *file, Source *src, GenericParameters *gp,
        BufferSpecification *bs, FORTreeSpecification *spec, bool persist) {
    FORTree *fortree;

    /*define the general functions of the rstartree*/
    static const SpatialIndexInterface vtable = {fortree_get_type,
        fortree_insert, fortree_remove, fortree_update, fortree_search_ss,
        fortree_header_writer, fortree_destroy};
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    fortree = (FORTree*) lwalloc(sizeof (FORTree));
    memcpy(&fortree->base, &base, sizeof (base));

    fortree->type = FORTREE_TYPE;
    fortree->spec = spec;
    fortree->info = rtreesinfo_create(0, 0, 0);
    fortree->current_node = NULL;

    //we have to persist the empty node
    if (persist) {
        fortree->current_node = rnode_create_empty();
        forb_create_new_rnode(&fortree->base, fortree->spec, 0, fortree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &fortree->base;
}
