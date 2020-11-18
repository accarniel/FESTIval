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

/* This file implements the R-tree index according to the original article from Guttman
 * Reference: GUTTMAN, A. R-trees: A dynamic index structure for spatial searching. SIGMOD Record,
ACM, New York, NY, USA, v. 14, n. 2, p. 47–57, 1984. 
 */

#include "rtree.h" //the basic library for rtree implementation
#include "split.h" //in order to split nodes...
#include "../main/log_messages.h" //messages errors
#include "../main/math_util.h" //to calculate aproximations with double values

#include "../fast/fast_buffer.h" //for FAST indices

#include "../efind/efind_buffer_manager.h" //for eFIND indices
#include "../efind/efind_read_buffer_policies.h"

#include "../main/header_handler.h" //for header storage

#include "../main/storage_handler.h" //for the tree height update

#include "../main/statistical_processing.h" // in order to collect statistical data

/*we need this function in order to make this R-tree index "FASTable"
 that is, in order to be used as a FAST index*/
static FASTSpecification *fast_spc;

void rtree_set_fastspecification(FASTSpecification *fesp) {
    fast_spc = fesp;
}

/*we need this function in order to make this R-tree an eFIND R-tree index
 that is, in order to be used as a eFIND index*/
static eFINDSpecification *efind_spc;

void rtree_set_efindspecification(eFINDSpecification *fesp) {
    efind_spc = fesp;
}

/*recursive search for the r-tree, such that specified in the original R-tree paper.*/
static SpatialIndexResult *recursive_search(RTree *rtree,
        const BBox *query,
        uint8_t predicate,
        int height,
        SpatialIndexResult *result);

/*this function choose the best node to add the new entry in a determined height
 it returns a copy of a RNode */
static RNode *choose_node(RTree *rtree, REntry *input, int height, RNodeStack *stack, int *chosen_address);
/* the same function of the R-TREE paper*/
static RNode *adjust_tree(RTree *rtree, RNode *l, RNode *ll, int *split_address, int l_height, RNodeStack *stack);
/* functions to insert a new entry into rtree in a determined height. 
 * The input entry is freed in this function.*/
static void insert_entry(RTree *rtree, REntry *input, int height);
/*function to condense the tree after a remotion*/
static void condense_tree(RTree *rtree, RNode *l, RNodeStack *stack, RNodeStack *removed_nodes, bool reinsert);

SpatialIndexResult *recursive_search(RTree *rtree,
        const BBox *query,
        uint8_t predicate,
        int height,
        SpatialIndexResult *result) {
    RNode *node;
    int i;
    uint8_t p;

    /* we copy the current node for backtracking purposes 
     that is, in order to follow several positive paths in the tree*/
    node = rnode_clone(rtree->current_node);

    /*internal node
     * let T = rtree->current_node, S = query
     S1 [Search subtrees] If T is not a leaf, check each entry E to determine
whether EI overlaps S. For all overlapping entries, invoke Search on the tree
whose root node is pointed to by Ep
     Note that we improve it by using the next comment.*/
    if (height != 0) {
        for (i = 0; i < rtree->current_node->nofentries; i++) {
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

            if (bbox_check_predicate(query, rtree->current_node->entries[i]->bbox, p)) {
                //we get the node in which the entry points to
                if (rtree->type == CONVENTIONAL_RTREE)
                    rtree->current_node = get_rnode(&rtree->base,
                        rtree->current_node->entries[i]->pointer, height - 1);
                else if (rtree->type == FAST_RTREE_TYPE)
                    rtree->current_node = (RNode *) fb_retrieve_node(&rtree->base,
                        rtree->current_node->entries[i]->pointer, height - 1);
                else if (rtree->type == eFIND_RTREE_TYPE)
                    rtree->current_node = (RNode *) efind_buf_retrieve_node(&rtree->base, efind_spc,
                        rtree->current_node->entries[i]->pointer, height - 1);
                else //it should not happen
                    _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);

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

                result = recursive_search(rtree, query, predicate, height - 1, result);

                /*after to traverse this child, we need to back 
                 * the reference of the current_node for the original one */
                rnode_copy(rtree->current_node, node);
            }
        }
    }/* leaf node 
      S2 [Search leaf nodes] If T is a leaf, check all entries E to determine
whether EI overlaps S. If so, E is a qualifying record
      Note that we improve it by using the next comment.*/
    else {
        for (i = 0; i < rtree->current_node->nofentries; i++) {

#ifdef COLLECT_STATISTICAL_DATA
            _processed_entries_num++;
#endif

            /*  * We employ MBRs relationships, like defined in: 
             * CLEMENTINI, E.; SHARMA, J.; EGENHOFER, M. J. Modelling topological spatial relations:
 Strategies for query processing. Computers & Graphics, v. 18, n. 6, p. 815–822, 1994.*/
            if (bbox_check_predicate(query, rtree->current_node->entries[i]->bbox, predicate)) {
                spatial_index_result_add(result, rtree->current_node->entries[i]->pointer);
            }
        }
    }
    rnode_free(node);
    return result;
}

RNode *choose_node(RTree *rtree, REntry *input, int h, RNodeStack *stack, int *chosen_address) {
    RNode *n = NULL;

    int tree_height;

    int i;
    int entry;
    double enlargement, aux;

    /*CL1 [Initialize] Set N to be the root node */
    n = rnode_clone(rtree->current_node); //because our stack that stores references
    *chosen_address = rtree->info->root_page;
    tree_height = rtree->info->height;

    /*CL2 [Leaf check ] If N is a leaf, return N.*/
    while (true) {
        if (n == NULL) {
            _DEBUG(ERROR, "Node is null in choose_node");
        }
        //yay we found the node N
        if (tree_height == h) {
            return n;
        }

        //    _DEBUGF(NOTICE, "choosing node, we are on the height %d ", h);
        //    rnode_print(n, *chosen_address);

        /*CL3. [Choose subtree ] If N is not a leaf,
let F be the entry in N whose rectangle FI needs least enlargement to
include EI. Resolve ties by choosing the entry with the rectangle of smallest area*/
        enlargement = bbox_area_of_required_expansion(input->bbox, n->entries[0]->bbox);
        entry = 0;
        for (i = 1; i < n->nofentries; i++) {
            aux = bbox_area_of_required_expansion(input->bbox, n->entries[i]->bbox);
            //the entry i is better than the previous one
            if (aux < enlargement) {
                enlargement = aux; //we update the least enlargement
                entry = i;
            } else if (DB_IS_EQUAL(aux, enlargement)) { /*there is a tie*/
                /*therefore, we choose the entry of smallest area */
                if (bbox_area(n->entries[i]->bbox) < bbox_area(n->entries[entry]->bbox)) {
                    enlargement = aux;
                    entry = i;
                }
            }
        }

#ifdef COLLECT_STATISTICAL_DATA
        _processed_entries_num += n->nofentries;
#endif

        //OK we have choose the better path, 
        //then we put it in our stack to adjust this node after
        rnode_stack_push(stack, n, *chosen_address, entry);

        /*CL4 [Descend until a leaf is reached.] Set N to be the child node pointed to by
Fp and repeat from CL2*/
        *chosen_address = n->entries[entry]->pointer;
        if (rtree->type == CONVENTIONAL_RTREE)
            n = get_rnode(&rtree->base, n->entries[entry]->pointer, tree_height - 1);
        else if (rtree->type == FAST_RTREE_TYPE)
            n = (RNode *) fb_retrieve_node(&rtree->base, n->entries[entry]->pointer, tree_height - 1);
        else if (rtree->type == eFIND_RTREE_TYPE)
            n = (RNode *) efind_buf_retrieve_node(&rtree->base, efind_spc,
                n->entries[entry]->pointer, tree_height - 1);
        else
            _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);

#ifdef COLLECT_STATISTICAL_DATA
        if (tree_height - 1 != 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(tree_height - 1, 1);
#endif

        tree_height--;
    }
    _DEBUG(ERROR, "Oops, no node was chosen in choose_node.");
    return NULL;
}

RNode *adjust_tree(RTree *rtree, RNode *l, RNode *ll, int *split_address, int l_height, RNodeStack *stack) {
    BBox *n_bbox;
    int parent_add;
    int entry;

    int h = l_height; //this is the height of the node to be inserted
    RNode *n = NULL, *nn = NULL;
    bool adjusting = true;

    /*AT1 [Initialize.] Set N=L If L was split previously, set NN to be the resulting
second node */
    n = rnode_clone(l);
    nn = rnode_clone(ll);

    rnode_free(rtree->current_node);
    rtree->current_node = NULL;

    /*AT2 [Check If done ] If N is the root, stop*/
    while (h != rtree->info->height) {
        /*AT3 [Adjust covering rectangle in parent entry] Let P be the parent node of
N, and let EN be N’s entry in P Adjust ENI so that it tightly encloses
all entry rectangles in N.*/
        /*if we are not adjusting the tree anymore because it is not more necessary
         we stop!*/
        if (!adjusting)
            break;

        //current_node will be the parent of n        
        rtree->current_node = rnode_stack_pop(stack, &parent_add, &entry);
        n_bbox = rnode_compute_bbox(n);

        //    _DEBUGF(NOTICE, "adjusting node, we are on the height %d ", h);
        //    rnode_print(rtree->current_node, parent_add);

        //is there a split node?
        /*in negative case, we maybe change only the BBOX of the parent*/
        /*in positive case, we change the BBOX of the parent and add the new entry*/
        if (nn->nofentries == 0) {
            //we check if it is necessary to modify the BBOX of this parent
            if (!bbox_check_predicate(n_bbox, rtree->current_node->entries[entry]->bbox, EQUAL)) {
                memcpy(rtree->current_node->entries[entry]->bbox, n_bbox, sizeof (BBox));

                //    _DEBUGF(NOTICE, "adjusted the entry %d", entry);

                if (rtree->type == CONVENTIONAL_RTREE) {
                    put_rnode(&rtree->base, rtree->current_node, parent_add, h + 1);
                } else if (rtree->type == FAST_RTREE_TYPE) {
                    fb_put_mod_bbox(&rtree->base, fast_spc, parent_add, bbox_clone(n_bbox), entry, h + 1);
                } else if (rtree->type == eFIND_RTREE_TYPE) {
                    efind_buf_mod_node(&rtree->base, efind_spc, parent_add,
                            (void*) rentry_clone(rtree->current_node->entries[entry]), h + 1);
                } else {
                    _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
                }

#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num++;
                insert_writes_per_height(h + 1, 1);
#endif
                adjusting = true;

                //we move up to next level (see T5 below)
                rnode_free(n);
                n = rtree->current_node;
                rtree->current_node = NULL;
            } else {
                adjusting = false;
            }

        } else {
            /*T4 [Propagate node split upward] If N has a partner NN resulting from an
    earlier split, create a new entry ENN with ENNp pointing to NN and ENNI 
    enclosing all rectangles in NN 
             * Add ENN to P If there is room 
             * Otherwise, invoke SplitNode to produce P and PP containing ENN and all P’s old
    entries*/
            BBox *bbox_split;

            //we update the bbox of the parent since it changed because of split
            memcpy(rtree->current_node->entries[entry]->bbox, n_bbox, sizeof (BBox));
            //we compute the bbox of the split node
            bbox_split = rnode_compute_bbox(nn);

            //we add the entry here without checking
            rnode_add_rentry(rtree->current_node, rentry_create(*split_address, bbox_split));

            /*AT5 [Move up to next level.] Set N=P and
set NN=PP If a split occurred, Repeat from AT2.*/
            //if there is a room
            if (rtree->current_node->nofentries <= rtree->spec->max_entries_int_node) {
                //    _DEBUGF(NOTICE, "adjusted the entry %d and added a new entry", entry);

                //then we write this node
                if (rtree->type == CONVENTIONAL_RTREE)
                    put_rnode(&rtree->base, rtree->current_node, parent_add, h + 1);
                else if (rtree->type == FAST_RTREE_TYPE) {
                    //we put the modification of the bbox
                    fb_put_mod_bbox(&rtree->base, fast_spc, parent_add, bbox_clone(n_bbox), entry, h + 1);
                    //we put the new pointer and new bbox for the split node
                    fb_put_mod_pointer(&rtree->base, fast_spc, parent_add, *split_address, rtree->current_node->nofentries - 1, h + 1);
                    fb_put_mod_bbox(&rtree->base, fast_spc, parent_add, bbox_clone(bbox_split), rtree->current_node->nofentries - 1, h + 1);
                } else if (rtree->type == eFIND_RTREE_TYPE) {
                    //we put the modification of the bbox
                    efind_buf_mod_node(&rtree->base, efind_spc, parent_add,
                            (void *) rentry_clone(rtree->current_node->entries[entry]), h + 1);
                    //we put the new entry which points to the split node
                    efind_buf_mod_node(&rtree->base, efind_spc, parent_add,
                            (void *) rentry_create(*split_address, bbox_clone(bbox_split)), h + 1);
                } else {
                    _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
                }

#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num++;
                insert_writes_per_height(h + 1, 1);
#endif
                //we did not perform split
                rnode_free(n);
                rnode_free(nn);
                n = rtree->current_node;
                nn = rnode_create_empty();
                rtree->current_node = NULL;
            } else {
                int cp = 0;
                if (rtree->type == FAST_RTREE_TYPE)
                    cp = rtree->current_node->nofentries;

                rnode_free(n);
                rnode_free(nn);

                n = rnode_create_empty();
                nn = rnode_create_empty();

                //we make a split of the parent node
                split_node(rtree->spec, rtree->current_node, h + 1, n, nn);

                //we update the parent to NULL since we made a split
                rnode_free(rtree->current_node);
                rtree->current_node = NULL;

                *split_address = rtreesinfo_get_valid_page(rtree->info);

                //    _DEBUG(NOTICE, "made split on the adjust tree");
                //    rnode_print(n, parent_add);
                //    rnode_print(nn, *split_address);

                if (rtree->type == CONVENTIONAL_RTREE) {
                    //we update the parent node since it was split
                    put_rnode(&rtree->base, n, parent_add, h + 1);
                    //we write the created node by the split with a new number page                
                    put_rnode(&rtree->base, nn, *split_address, h + 1);
                } else if (rtree->type == FAST_RTREE_TYPE) {
                    /*int in;
                    for (in = 0; in < n->nofentries; in++) {
                        //we removed the if statement here because the modification made on the parent should also be done here!
                        //we put the new pointer and new bbox for the split node
                        fb_put_mod_pointer(&rtree->base, fast_spc, parent_add, n->entries[in]->pointer, in, h + 1);
                        fb_put_mod_bbox(&rtree->base, fast_spc, parent_add, bbox_clone(n->entries[in]->bbox), in, h + 1);
                    }
                    //we need to 'del' the remaining entries in the reverse order because of the buffer
                    for (in = cp - 2; in >= n->nofentries; in--) {
                        fb_put_mod_bbox(&rtree->base, fast_spc, parent_add, NULL, in, h + 1);
                    }
                    //we put the new node in the buffer
                    fb_put_new_node(&rtree->base, fast_spc, *split_address,
                            (void *) rnode_clone(nn), h + 1);*/

                    //remove the old version of modified node
                    fb_del_node(&rtree->base, fast_spc, parent_add, h + 1);
                    //we put the node in the buffer
                    fb_put_new_node(&rtree->base, fast_spc, parent_add,
                            (void *) rnode_clone(n), h + 1);

                    //we put the new node in the buffer
                    fb_put_new_node(&rtree->base, fast_spc, *split_address,
                            (void *) rnode_clone(nn), h + 1);
                } else if (rtree->type == eFIND_RTREE_TYPE) {
                    int in;
                    //we update the parent node since it was split
                    /* eFIND do this as follows: 
                     (i) we del the split node and store its pointer;
                     * (ii) we create it (again) in the buffer;
                     (iii) we add back the new entries
                     the advantage of this strategy is that we will not store NULL values in the buffer
                     * further, it will remove any previous modification of this node
                     consequently we will improve the buffer space utilization*/
                    efind_buf_del_node(&rtree->base, efind_spc, parent_add, h + 1);
                    efind_buf_create_node(&rtree->base, efind_spc, parent_add, h + 1);
                    for (in = 0; in < n->nofentries; in++) {
                        efind_buf_mod_node(&rtree->base, efind_spc, parent_add,
                                (void *) rentry_clone(n->entries[in]), h + 1);
                    }

                    //we put the newly created node
                    efind_buf_create_node(&rtree->base, efind_spc, *split_address, h + 1);
                    for (in = 0; in < nn->nofentries; in++) {
                        efind_buf_mod_node(&rtree->base, efind_spc, *split_address,
                                (void *) rentry_clone(nn->entries[in]), h + 1);
                    }
                } else {
                    _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
                }

#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num += 2;
                insert_writes_per_height(h + 1, 2);
#endif
            }
        }

        lwfree(n_bbox);
        h++;
    }

    /*we stopped to adjusted the tree, we set the current_node as the root node here*/
    if (!adjusting) {
        while (stack->size > 0) {
            rnode_free(rtree->current_node);
            rtree->current_node = rnode_stack_pop(stack, &parent_add, &entry);
        }
        rnode_free(n);
    } else {
        rtree->current_node = n;
    }
    if (nn->nofentries > 0) {
        return nn;
    } else {
        rnode_free(nn);
    }
    return NULL;
}

void insert_entry(RTree *rtree, REntry *input, int height) {
    RNode *chosen_node; //the node in which was chosen to insert the input
    int chosen_address; //page number of the chosen_node

    int max_entries;

    RNode *l = rnode_create_empty(); //the normal node (that is, the L)
    RNode *ll = rnode_create_empty(); //the split node (that is, the LL)
    int split_address = -1; //page number of the split node
    RNodeStack *stack = rnode_stack_init(); //to adjust_Tree
    RNode *new = NULL; //in case of the split occurred until the root node

    if (height == 0) {
        max_entries = rtree->spec->max_entries_leaf_node;
    } else {
        max_entries = rtree->spec->max_entries_int_node;
    }

    // _DEBUG(NOTICE, "inserting a new entry");

    /*I1 [Find position for new record ] Invoke ChooseLeaf to select a leaf
 node L in which to place E --> L in this case is our chosen_node
     In this case, ChooseLeaf is the choose_node which will select the node with a specific level
     e.g., 0 for leaf nodes or greater than 0 (to reinsert from the delete operation)*/
    chosen_node = choose_node(rtree, input, height, stack, &chosen_address);

    //    _DEBUG(NOTICE, "node chosen");
    //    rnode_print(chosen_node, chosen_address);

    //we add the entry here without checking
    rnode_add_rentry(chosen_node, input);

    /*I2 [Add record to leaf node ] If L has room for another entry, install E
Otherwise invoke SplitNode to obtain L and LL containing E and all the
old entrees of L
     That is: if the new entry E does not fit in chosen_node, then we obtain L and LL */
    if (chosen_node->nofentries <= max_entries) {
        //    _DEBUG(NOTICE, "the node chosen has enough space to insert the new entry");
        /*then we can write the node with the new entry*/
        if (rtree->type == CONVENTIONAL_RTREE) {
            put_rnode(&rtree->base, chosen_node, chosen_address, height);
        } else if (rtree->type == FAST_RTREE_TYPE) {
            fb_put_mod_pointer(&rtree->base, fast_spc, chosen_address, input->pointer, chosen_node->nofentries - 1, height);
            fb_put_mod_bbox(&rtree->base, fast_spc, chosen_address, bbox_clone(input->bbox), chosen_node->nofentries - 1, height);
        } else if (rtree->type == eFIND_RTREE_TYPE) {
            efind_buf_mod_node(&rtree->base, efind_spc, chosen_address,
                    (void *) rentry_clone(input), height);
        } else {
            _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
        }

#ifdef COLLECT_STATISTICAL_DATA
        if (height != 0)
            _written_int_node_num++;
        else
            _written_leaf_node_num++;
        insert_writes_per_height(height, 1);
#endif       

    } else { /* we do not have space */
        //RNode *cp = NULL;
        //if (rtree->type == FAST_RTREE_TYPE)
        //    cp = rnode_clone(chosen_node);

        /*then we have to split the chosen_node that is exceeding its capacity*/
        split_node(rtree->spec, chosen_node, height, l, ll);
        //we update the chosen_node since it was changed by split
        rnode_copy(chosen_node, l);
        split_address = rtreesinfo_get_valid_page(rtree->info);

        //    _DEBUG(NOTICE, "split was needed");
        //    rnode_print(l, chosen_address);
        //    rnode_print(ll, split_address);

        if (rtree->type == CONVENTIONAL_RTREE) {
            //we update the chosen_node to l since it was split
            put_rnode(&rtree->base, l, chosen_address, height);
            //we write the created node by the split with a new number page        
            put_rnode(&rtree->base, ll, split_address, height);
        } else if (rtree->type == FAST_RTREE_TYPE) {
            /*
            int in;
            for (in = 0; in < l->nofentries; in++) {
                if (l->entries[in]->pointer != cp->entries[in]->pointer) {
                    //we put the new pointer and new bbox for the split node
                    fb_put_mod_pointer(&rtree->base, fast_spc, chosen_address, l->entries[in]->pointer, in, height);
                    fb_put_mod_bbox(&rtree->base, fast_spc, chosen_address, bbox_clone(l->entries[in]->bbox), in, height);
                }
            }
            //we need to 'del' the remaining entries in the reverse order to avoid holes in the node
            //it is "-2" here since in the buffer the new entry was not inserted!
            for (in = cp->nofentries - 2; in >= l->nofentries; in--) {
                fb_put_mod_bbox(&rtree->base, fast_spc, chosen_address, NULL, in, height);
            }

            //we put the new node in the buffer
            fb_put_new_node(&rtree->base, fast_spc, split_address,
                    (void *) rnode_clone(ll), height);
            rnode_free(cp);*/

            /*it is more efficient if we remove the old version and create the modified node again*/

            //remove the old version of chosen_node
            fb_del_node(&rtree->base, fast_spc, chosen_address, height);
            //we put the node in the buffer
            fb_put_new_node(&rtree->base, fast_spc, chosen_address,
                    (void *) rnode_clone(l), height);

            //we put the new node in the buffer
            fb_put_new_node(&rtree->base, fast_spc, split_address,
                    (void *) rnode_clone(ll), height);
        } else if (rtree->type == eFIND_RTREE_TYPE) {
            int in;
            //we update the chosen node since it was split
            /* eFIND do this as follows: 
             (i) we del the split node and store its pointer;
             * (ii) we create it (again) in the buffer;
             (iii) we add back the new entries
             the advantage of this strategy is that we will not store NULL values in the buffer
             * further, it will remove any previous modification of this node
             consequently we will improve the buffer space utilization*/
            efind_buf_del_node(&rtree->base, efind_spc, chosen_address, height);
            efind_buf_create_node(&rtree->base, efind_spc, chosen_address, height);
            for (in = 0; in < l->nofentries; in++) {
                efind_buf_mod_node(&rtree->base, efind_spc, chosen_address,
                        (void *) rentry_clone(l->entries[in]), height);
            }

            //we put the newly created node
            efind_buf_create_node(&rtree->base, efind_spc, split_address, height);
            for (in = 0; in < ll->nofentries; in++) {
                efind_buf_mod_node(&rtree->base, efind_spc, split_address,
                        (void *) rentry_clone(ll->entries[in]), height);
            }
        } else {
            _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
        }

        //_DEBUG(NOTICE, "split done");


#ifdef COLLECT_STATISTICAL_DATA
        if (height != 0)
            _written_int_node_num += 2;
        else
            _written_leaf_node_num += 2;
        insert_writes_per_height(height, 2);
#endif
    }

    /*I3 [Propagate changes upward] Invoke AdjustTree on L, also passing LL If a
    split was performed.
     We employ a stack here to do it
     the adjust_tree also sets the rtree->current_node to the root node of the tree
     the split_address now stores the address of the new node if the root node was also split*/
    new = adjust_tree(rtree, chosen_node, ll, &split_address, height, stack);

    /*I4 [Grow tree taller ] If node split propagation caused the root to split,
create a new root whose children are the two resulting nodes */
    if (new != NULL) {
        //then we have to create a new root with 2 entries
        RNode *new_root = rnode_create_empty();
        int new_root_add;

        //first entry is the current_node, which is the old root,
        //while the second entry is the split node
        REntry *entry1, *entry2;

        //we allocate one more page for the new root
        new_root_add = rtreesinfo_get_valid_page(rtree->info);

        //the height of the tree is incremented
        rtree->info->height++;

        entry1 = rentry_create(rtree->info->root_page, rnode_compute_bbox(rtree->current_node));
        entry2 = rentry_create(split_address, rnode_compute_bbox(new));

        //we add the new two entries into the new root node
        rnode_add_rentry(new_root, entry1);
        rnode_add_rentry(new_root, entry2);

        //    _DEBUG(NOTICE, "Creating a new root node");
        //    rnode_print(new_root, new_root_add);

        //we write the new root node
        if (rtree->type == CONVENTIONAL_RTREE) {
            put_rnode(&rtree->base, new_root, new_root_add, rtree->info->height);
        } else if (rtree->type == FAST_RTREE_TYPE) {
            fb_put_new_node(&rtree->base, fast_spc, new_root_add,
                    (void *) rnode_clone(new_root), rtree->info->height);
        } else if (rtree->type == eFIND_RTREE_TYPE) {
            //we firstly set the new height of the tree
            if (efind_spc->read_buffer_policy == eFIND_HLRU_RBP)
                efind_readbuffer_hlru_set_tree_height(rtree->info->height);

            efind_buf_create_node(&rtree->base, efind_spc, new_root_add, rtree->info->height);
            efind_buf_mod_node(&rtree->base, efind_spc, new_root_add,
                    (void *) rentry_clone(entry1), rtree->info->height);
            efind_buf_mod_node(&rtree->base, efind_spc, new_root_add,
                    (void *) rentry_clone(entry2), rtree->info->height);
        } else {
            _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
        }
        storage_update_tree_height(&rtree->base, rtree->info->height);

        //_DEBUG(NOTICE, "New root node created");

#ifdef COLLECT_STATISTICAL_DATA
        _written_int_node_num++;
        insert_writes_per_height(rtree->info->height, 1);
#endif

        //we update the root page
        rtree->info->root_page = new_root_add;

        rnode_free(rtree->current_node);
        rtree->current_node = new_root;
        rnode_free(new);
    }
    rnode_free(chosen_node);
    rnode_free(l); //note: it was allocated
    rnode_free(ll); //note: it was allocated
    rnode_stack_destroy(stack);
}

/*we have to reinsert the removed nodes? if true, then we have to add it
otherwise, we don't reinsert and the caller is responsible to do it*/
void condense_tree(RTree *rtree, RNode *l, RNodeStack *stack, RNodeStack *removed_nodes, bool reinsert) {
    int cur_height; //the current height of the node
    int height; //the height of the tree
    int parent_add; //address of the parent of n
    int parent_entry, i;
    BBox *bbox;
    RNode *n;

    bool adjusting = true;
    bool removed = true;

    /*CT1 [Initialize ] Set N=L Set Q, the set of eliminated nodes, to be empty*/
    n = rnode_clone(l);
    rnode_free(rtree->current_node);
    rtree->current_node = NULL; //this will be the parent of n
    parent_entry = 0;
    cur_height = 0; //the current height (height == 0 means leaf node)
    height = rtree->info->height; //the height of the tree 

    /*CT2 [Find parent entry.] If N is the root, go to CT6
     * Otherwise let P be the parent of N, and let EN be N’s entry IIlP*/
    while (cur_height != height) {
        if (!adjusting)
            break;

        rtree->current_node = rnode_stack_pop(stack, &parent_add, &parent_entry);

        /*CT3 [Eliminate under-full node.] If N has fewer than m entries, delete EN from
P and add N to set Q.*/
        if ((cur_height == 0 && n->nofentries < rtree->spec->min_entries_leaf_node) ||
                (cur_height != 0 && n->nofentries < rtree->spec->min_entries_int_node)) {
            int removed_entry_pointer = rtree->current_node->entries[parent_entry]->pointer;

            if (rtree->type == CONVENTIONAL_RTREE) {
                //we remove this node from the index file (the n)
                del_rnode(&rtree->base, removed_entry_pointer, cur_height);
            } else if (rtree->type == FAST_RTREE_TYPE) {
                fb_del_node(&rtree->base, fast_spc, removed_entry_pointer, cur_height);
            } else if (rtree->type == eFIND_RTREE_TYPE) {
                efind_buf_del_node(&rtree->base, efind_spc, removed_entry_pointer, cur_height);
            } else {
                _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
            }

            //we add the removed page as an empty page now
            rtreesinfo_add_empty_page(rtree->info, removed_entry_pointer);

            //we store it in other stack in order to reinsert its entries in the future
            rnode_stack_push(removed_nodes, n, cur_height, -1);
            //we remove the corresponding parent entry
            rnode_remove_rentry(rtree->current_node, parent_entry);
            removed = true;

            //we remove it here because we need to know the position or the RRN of the entry
            if (rtree->type == FAST_RTREE_TYPE) {
                fb_put_mod_bbox(&rtree->base, fast_spc, parent_add, NULL, parent_entry, cur_height + 1);
            } else if (rtree->type == eFIND_RTREE_TYPE) {
                efind_buf_mod_node(&rtree->base, efind_spc, parent_add,
                        rentry_create(removed_entry_pointer, NULL), cur_height + 1);
            } else //we update because this node here is the root node
                if (rtree->type == CONVENTIONAL_RTREE && (cur_height + 1) == rtree->info->height) {
                put_rnode(&rtree->base, rtree->current_node, parent_add, cur_height + 1);
            }

            // _DEBUG(NOTICE, "Underflow processed");

#ifdef COLLECT_STATISTICAL_DATA
            if (cur_height != 0) {
                //we removed an internal node, then we add it
                _deleted_int_node_num++;
            } else {
                //we removed a leaf node
                _deleted_leaf_node_num++;
            }
            insert_writes_per_height(cur_height, 1);
            _written_int_node_num++;
            insert_writes_per_height(cur_height + 1, 1);
#endif
        } else {
            /*CT4 [Adjust covering rectangle ] If N has not been eliminated, 
             * adjust ENI to tightly contain all entries in N*/
            bbox = rnode_compute_bbox(n);

            //we update n since it has one less entry
            if (rtree->type == CONVENTIONAL_RTREE && removed) {
                put_rnode(&rtree->base, n,
                        rtree->current_node->entries[parent_entry]->pointer, cur_height);
#ifdef COLLECT_STATISTICAL_DATA
                if (cur_height == 0)
                    _written_leaf_node_num++;
                else
                    _written_int_node_num++;
                insert_writes_per_height(cur_height, 1);
#endif
            }

            removed = false;

            //check if we need to adjust the parent entry
            if (!bbox_check_predicate(bbox, rtree->current_node->entries[parent_entry]->bbox, EQUAL)) {
                memcpy(rtree->current_node->entries[parent_entry]->bbox, bbox, sizeof (BBox));

                if (rtree->type == CONVENTIONAL_RTREE) {
                    put_rnode(&rtree->base, rtree->current_node, parent_add, cur_height + 1);
                } else if (rtree->type == FAST_RTREE_TYPE) {
                    fb_put_mod_bbox(&rtree->base, fast_spc, parent_add, bbox_clone(bbox), parent_entry, cur_height + 1);
                } else if (rtree->type == eFIND_RTREE_TYPE) {
                    efind_buf_mod_node(&rtree->base, efind_spc, parent_add,
                            (void *) rentry_clone(rtree->current_node->entries[parent_entry]), cur_height + 1);
                } else {
                    _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
                }

#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num++;
                insert_writes_per_height(cur_height + 1, 1);
#endif
                adjusting = true;
            } else {
                adjusting = false;
            }
            rnode_free(n);

            lwfree(bbox);
            bbox = NULL;
        }
        /*CT5 [Move up one level in tree ] Set N=P
and repeat from CT2.*/
        n = rtree->current_node;
        rtree->current_node = NULL;
        cur_height++; //we up one more level until root
    }

    /*we stopped to adjusted the tree, we set the current_node as the root node here*/
    if (!adjusting) {
        //if we have more elements in the stack because if we stop to adjust in the root node
        if (stack->size > 0) {
            while (stack->size > 0) {
                rnode_free(rtree->current_node);
                rtree->current_node = rnode_stack_pop(stack, &parent_add, &parent_entry);
            }
            rnode_free(n);
        } else {
            rtree->current_node = n;
        }
    } else {
        //we update the root node
        rtree->current_node = n;
    }

    /*CT6 [Re-insert orphaned entries ] Reinsert all entries of nodes in set Q
Entries from eliminated leaf nodes are reinserted in tree leaves as
described in Algorithm Insert, but entries from higher-level nodes must
be placed higher in the tree, so that leaves of their dependent subtrees
will be on the same level as leaves of the main tree*/
    if (reinsert) {
        while (removed_nodes->size > 0) {
            n = rnode_stack_pop(removed_nodes, &cur_height, NULL);
            for (i = 0; i < n->nofentries; i++) {
                //we have to pass COPIES of the entries since this function destroy it
                insert_entry(rtree, rentry_clone(n->entries[i]), cur_height);
            }
            rnode_free(n);
        }
    }
}

/*default searching algorithm of the R-tree (defined in rtree.h)*/
SpatialIndexResult *rtree_search(RTree *rtree, const BBox *search, uint8_t predicate) {
    SpatialIndexResult *sir = spatial_index_result_create();
    /* current node here MUST be equal to the root node */
    if (rtree->current_node != NULL) {
        sir = recursive_search(rtree, search, predicate, rtree->info->height, sir);
    }
    return sir;
}

/*default algorithm to remove an entry in a R-tree (defined in rtree.h)*/
bool rtree_remove_with_removed_nodes(RTree *rtree, const REntry *to_remove, RNodeStack *removed_nodes, bool reinsert) {

    /*D1 [Find node containing record ] Invoke FindLeaf to locate the leaf
node L containing E Stop if the record was not found.*/

    /*the FindLeaf is inlined here in order to do this in an iterative way */

    RNodeStack *stack;
    int i, entry;
    int parent_add;
    int found_index;
    bool inside;
    RNode *n = NULL;
    RNode *found_node = NULL;
    int h;

    found_index = -1;
    inside = false;
    stack = rnode_stack_init();
    h = rtree->info->height;

    rnode_stack_push(stack, rnode_clone(rtree->current_node), rtree->info->root_page, -1);

    //_DEBUG(NOTICE, "Searching subtrees");

    /*FL1. [Search subtrees ] If T is not a leaf, check each entry F in T 
     * to determine if FI overlaps EI For each such entry invoke FindLeaf on the
tree whose root is pointed to by Fp until E is found or all entries have
been checked*/
    while (found_index == -1 && stack->size > 0) {
        n = rnode_stack_peek(stack, &parent_add, &entry);
        entry += 1;
        inside = false;
        //it is an internal node
        if (h != 0) {
            for (i = entry; i < n->nofentries; i++) {
#ifdef COLLECT_STATISTICAL_DATA
                _processed_entries_num++;
#endif
                //check if it is a good entry
                if (bbox_check_predicate(to_remove->bbox, n->entries[i]->bbox, INSIDE_OR_COVEREDBY)) {
                    //if yes, we need to update the chosen entry
                    stack->top->entry_of_parent = i;

                    //next, we read the node whose this entry points to
                    parent_add = n->entries[i]->pointer;

                    if (rtree->type == CONVENTIONAL_RTREE)
                        n = get_rnode(&rtree->base, n->entries[i]->pointer, h - 1);
                    else if (rtree->type == FAST_RTREE_TYPE)
                        n = (RNode *) fb_retrieve_node(&rtree->base, n->entries[i]->pointer, h - 1);
                    else if (rtree->type == eFIND_RTREE_TYPE)
                        n = (RNode *) efind_buf_retrieve_node(&rtree->base, efind_spc,
                            n->entries[i]->pointer, h - 1);
                    else
                        _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);

#ifdef COLLECT_STATISTICAL_DATA
                    if (h - 1 != 0) {
                        //we visited an internal node, then we add it
                        _visited_int_node_num++;
                    } else {
                        //we visited a leaf node
                        _visited_leaf_node_num++;
                    }
                    insert_reads_per_height(h - 1, 1);
#endif

                    //put it in our stack - note that we don't know which entry we have to choose yet               
                    rnode_stack_push(stack, n, parent_add, -1);
                    inside = true;
                    h--;
                    break; //go to the next iteration of while (the continue)
                }
            }
            if (inside) {
                continue;
            }
        } else {//leaf node
            /*FL2. [Search leaf node for record ] If T is
a leaf, check each entry to see if it matches E If E is found return T*/
            for (i = 0; i < n->nofentries; i++) {
#ifdef COLLECT_STATISTICAL_DATA
                _processed_entries_num++;
#endif
                if (to_remove->pointer == n->entries[i]->pointer) {
                    found_index = i;
                    found_node = rnode_clone(n);
                    break;
                }
            }
        }

        //we remove from stack since (i) it was a failed entry of the parent of a leaf node
        //(ii) or this rtree has only one node (root node is a leaf node)
        //(iii) and to remove the leaf node from the stack since it is not an internal node
        rnode_stack_pop_without_return(stack);
        h++;

    }
    //_DEBUG(NOTICE, "Done");

    /*D2 [Delete record.] Remove E from L*/
    if (found_index != -1 && found_node != NULL) {
        //_DEBUG(NOTICE, "Removing the entry");

        rnode_remove_rentry(found_node, found_index);
        //we update the buffer (it is done here because we need to know the position)
        if (rtree->type == FAST_RTREE_TYPE) {
            fb_put_mod_bbox(&rtree->base, fast_spc, parent_add, NULL, found_index, 0);
        } else if (rtree->type == eFIND_RTREE_TYPE) {
            efind_buf_mod_node(&rtree->base, efind_spc, parent_add, rentry_create(to_remove->pointer, NULL), 0);
        }
        //we update n here, and not in condense_tree, when the height of the tree is 0
        if (rtree->type == CONVENTIONAL_RTREE && rtree->info->height == 0) {
            put_rnode(&rtree->base, n, parent_add, 0);
        }
        //_DEBUG(NOTICE, "Propagating the changes");
        /*D3 [Propagate changes ] Invoke CondenseTree, passing L*/
        condense_tree(rtree, found_node, stack, removed_nodes, reinsert);
        rnode_free(found_node);
        //_DEBUG(NOTICE, "Done");
#ifdef COLLECT_STATISTICAL_DATA
        if (rtree->info->height == 0) {
            _written_leaf_node_num++;
            insert_writes_per_height(0, 1);
        }
#endif

    }

    /*D4 [Shorten tree.] If the root node has only one child after the tree has
been adjusted, make the child the new root*/
    if (reinsert && rtree->current_node->nofentries == 1 && rtree->info->height > 0) {
        int p = rtree->current_node->entries[0]->pointer;
        RNode *new_root = NULL;

        //_DEBUG(NOTICE, "We have to cut the tree");

        if (rtree->type == CONVENTIONAL_RTREE) {
            del_rnode(&rtree->base, rtree->info->root_page, rtree->info->height);
        } else if (rtree->type == FAST_RTREE_TYPE) {
            fb_del_node(&rtree->base, fast_spc, rtree->info->root_page, rtree->info->height);
        } else if (rtree->type == eFIND_RTREE_TYPE) {
            //we update the height of the tree
            if (efind_spc->read_buffer_policy == eFIND_HLRU_RBP)
                efind_readbuffer_hlru_set_tree_height(rtree->info->height - 1);
            efind_buf_del_node(&rtree->base, efind_spc,
                    rtree->info->root_page, rtree->info->height);
        } else {
            _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
        }
        storage_update_tree_height(&rtree->base, rtree->info->height - 1);
        rnode_free(rtree->current_node);

        //we add the removed page as an empty page now
        rtreesinfo_add_empty_page(rtree->info, rtree->info->root_page);

#ifdef COLLECT_STATISTICAL_DATA
        _deleted_int_node_num++;
        insert_writes_per_height(rtree->info->height, 1);
#endif

        rtree->info->root_page = p;

        if (rtree->type == CONVENTIONAL_RTREE)
            new_root = get_rnode(&rtree->base, p, rtree->info->height - 1);
        else if (rtree->type == FAST_RTREE_TYPE)
            new_root = (RNode *) fb_retrieve_node(&rtree->base, p, rtree->info->height - 1);
        else if (rtree->type == eFIND_RTREE_TYPE) {
            new_root = (RNode *) efind_buf_retrieve_node(&rtree->base, efind_spc,
                    p, rtree->info->height - 1);
        }

#ifdef COLLECT_STATISTICAL_DATA
        if (rtree->info->height > 1) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(rtree->info->height - 1, 1);
#endif

        rtree->current_node = new_root;
        rtree->info->height--;

        //_DEBUG(NOTICE, "done");
    }

    rnode_stack_destroy(stack);

    if (found_index != -1)
        return true;
    else
        return false;
}

/*********************************
 * functions in order to make RTree a standard SpatialIndex (see spatial_index.h)
 *********************************/
static uint8_t rtree_get_type(const SpatialIndex *si) {
    RTree *rtree = (void *) si;
    return rtree->type;
}

static bool rtree_insert(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    BBox *bbox = (BBox*) lwalloc(sizeof (BBox));
    RTree *rtree = (void *) si;
    REntry *input;

    gbox_to_bbox(geom->bbox, bbox);

    input = rentry_create(pointer, bbox);

    insert_entry(rtree, input, 0);

    return true;
}

static bool rtree_remove(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    BBox *bbox = (BBox*) lwalloc(sizeof (BBox));
    RTree *rtree = (void *) si;
    REntry *rem;
    RNodeStack *removed_nodes;
    bool ret;

    removed_nodes = rnode_stack_init();

    gbox_to_bbox(geom->bbox, bbox);
    rem = rentry_create(pointer, bbox);

    ret = rtree_remove_with_removed_nodes(rtree, rem, removed_nodes, true);

    rnode_stack_destroy(removed_nodes);
    lwfree(rem->bbox);
    lwfree(rem);

    return ret;
}

static bool rtree_update(SpatialIndex *si, int oldpointer, const LWGEOM *oldgeom, int newpointer, const LWGEOM *newgeom) {
    bool r, i = false;
    //TO-DO improve the return value of this.. i.e., it can return an error identifier.
    r = rtree_remove(si, oldpointer, oldgeom);
    if (r)
        i = rtree_insert(si, newpointer, newgeom);
    return r && i;
}

static SpatialIndexResult *rtree_search_ss(SpatialIndex *si, const LWGEOM *search_object, uint8_t predicate) {
    SpatialIndexResult *sir;
    BBox *search = (BBox*) lwalloc(sizeof (BBox));
    RTree *rtree = (void *) si;

    gbox_to_bbox(search_object->bbox, search);

    sir = rtree_search(rtree, search, predicate);

    lwfree(search);
    return sir;
}

static bool rtree_header_writer(SpatialIndex *si, const char *file) {
    festival_header_writer(file, CONVENTIONAL_RTREE, si);
    return true;
}

static void rtree_destroy(SpatialIndex *si) {
    RTree *rtree = (void *) si;
    rnode_free(rtree->current_node);
    lwfree(rtree->spec);
    rtreesinfo_free(rtree->info);

    generic_parameters_free(rtree->base.gp);
    lwfree(rtree->base.index_file);
    source_free(rtree->base.src);

    lwfree(rtree);
}

/* this function returns a rtree with height equal to 0
 *we only create and write the root node here if persist is equal to true! */
SpatialIndex *rtree_empty_create(char *file, Source *src, GenericParameters *gp,
        BufferSpecification *bs, bool persist) {
    RTree *rtree;

    /*define the general functions of the rtree*/
    static const SpatialIndexInterface vtable = {rtree_get_type,
        rtree_insert, rtree_remove, rtree_update, rtree_search_ss,
        rtree_header_writer, rtree_destroy};
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    rtree = (RTree*) lwalloc(sizeof (RTree));
    memcpy(&rtree->base, &base, sizeof (base));
    rtree->type = CONVENTIONAL_RTREE; //this is a conventional r-tree

    rtree->spec = (RTreeSpecification*) lwalloc(sizeof (RTreeSpecification));
    rtree->info = rtreesinfo_create(0, 0, 0);

    rtree->current_node = NULL;

    //we have to persist the empty node
    if (persist) {
        rtree->current_node = rnode_create_empty();
        put_rnode(&rtree->base, rtree->current_node, rtree->info->root_page, rtree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &rtree->base;
}
