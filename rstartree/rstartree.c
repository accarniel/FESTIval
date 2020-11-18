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

/* This file implements the R*-tree index according to the original article from Beckman
 * Reference: BECKMANN, N.; KRIEGEL, H.-P.; SCHNEIDER, R.; SEEGER, B. The R*-tree: An efficient
and robust access method for points and rectangles. SIGMOD Record, ACM, v. 19, n. 2, p.
322–331, 1990.
 */

#include <stdlib.h> //for qsort
#include <limits.h> //for the minimum and maximum int values
#include "rstartree.h" //for basic definitions
#include "../rtree/rnode_stack.h"
#include "../main/log_messages.h" //for logging
#include "../main/math_util.h" //for DB handling
#include "../rtree/split.h" //for split

#include "../fast/fast_buffer.h" //for fast-indices

#include "../efind/efind_buffer_manager.h" //for efind-indices
#include "../efind/efind_read_buffer_policies.h"

#include "../main/header_handler.h" //for header storage

#include "../main/storage_handler.h" //for the tree height update

#include "../main/statistical_processing.h" /* to collect statistical data */

/*we need this function/variable in order to make this R-tree index "FASTable"
 that is, in order to be used as FAST index*/
static FASTSpecification *fast_spc;

void rstartree_set_fastspecification(FASTSpecification *fesp) {
    fast_spc = fesp;
}

/*we need this function in order to make this R*-tree, an eFIND R*-tree index
 that is, in order to be used as a eFIND index*/
static eFINDSpecification *efind_spc;

void rstartree_set_efindspecification(eFINDSpecification *fesp) {
    efind_spc = fesp;
}

/*an auxiliary struct to store the required expansion and distance of entries in a node
 it is used in the choose_node_rstartree function
 It is also used in the reinsert operation*/
typedef struct {
    int entry;
    double value; //which will be a coordinate value
} Pair;
/*for the qsort (descendent order)*/
static int double_desc_comp(const void * elem1, const void * elem2);
/*for the qsort (ascendent order)*/
static int double_asc_comp(const void * elem1, const void * elem2);

int double_desc_comp(const void * elem1, const void * elem2) {
    Pair *f = (Pair*) elem1;
    Pair *s = (Pair*) elem2;
    if (DB_LT(f->value, s->value)) return 1;
    if (DB_GT(f->value, s->value)) return -1;
    return 0;
}

int double_asc_comp(const void * elem1, const void * elem2) {
    Pair *f = (Pair*) elem1;
    Pair *s = (Pair*) elem2;
    if (DB_GT(f->value, s->value)) return 1;
    if (DB_LT(f->value, s->value)) return -1;
    return 0;
}

/*this function choose a node for insert algorithm - 
 * the difference here if compared to R-tree is that this algorithm consider other factors
 like the overlapping area*/
static RNode *choose_node_rstartree(RStarTree *rstar, REntry *input, int i_height, RNodeStack *stack, int *chosen_address);
/*this function adjust the tree after an insertion - 
 * but it does not apply the split since this function is also used for the reinsertion*/
static void adjust_rstartree(RStarTree *rstar, RNode *chosen_node, int height, RNodeStack *stack);
/* reinsertion procedure of the r*-tree*/
static void reinsert_rstartree(RStarTree *rstar, RNode *chosen_node, int chosen_address, int height, RNodeStack *stack);
/*insert an entry in a determined height of the tree (height = 0 is an insert in a leaf node)*/
static void insert_entry_rstartree(RStarTree *rstar, REntry *input, int height);
/* original deletion algorithm from R-tree 
 * but it uses the insert_rstartree to reinsert the removed nodes
 */
static bool delete_entry_rstartree(RStarTree *rstar, const REntry *to_remove);

RNode *choose_node_rstartree(RStarTree *rstar, REntry *input, int i_height, RNodeStack *stack, int *chosen_address) {
    RNode *n;
    int height;
    int i;
    int entry = 0;
    double enlargement, aux;
    bool didfit = false;
    Pair *en = NULL;

    /*CS1 Set N to be the root node */
    n = rnode_clone(rstar->current_node); //because of our stack that stores references    
    *chosen_address = rstar->info->root_page;
    height = rstar->info->height;

    /*CS2 If N is a leaf, return N.*/
    while (true) {
        if (n == NULL) {
            _DEBUG(ERROR, "Node is null in choose_node");
        }
        //yay we found the node N
        if (height == i_height) {
            return n;
        }

        /*if this node points to leaf node then we will need an array to store
         the enlargement area for each entry of this node*/
        if (0 == height - 1) {
            en = (Pair*) lwalloc(sizeof (Pair) * n->nofentries);
        }

        /*[continuation of CS2] else
If the childpointers in N point to leaves [determine the minimum overlap cost],
choose the entry in N whose rectangle needs least overlap enlargement to include the new data
rectangle 
         * Resolve ties by choosing the entry whose rectangle needs least area enlargement,
then the entry with the rectangle of smallest area
if the childpointers in N do not point to leaves [determine the minimum area cost],
choose the entry in N whose rectangle needs least area enlargement to include the new data
rectangle Resolve ties by choosing the entry with the rectangle of smallest area
end*/
        enlargement = DBL_MAX;
        didfit = false;
        for (i = 0; i < n->nofentries; i++) {
            /*this refers to [determine the minimum area cost]*/
            aux = bbox_area_of_required_expansion(input->bbox, n->entries[i]->bbox);

            if (didfit) {
                /*we had a good choice, then we check if this current choice is better                  
                 */
                if (DB_IS_ZERO(aux)) { //this is a tie with the previous entry
                    /*therefore, we choose the entry of smallest area */
                    if (bbox_area(n->entries[i]->bbox) < bbox_area(n->entries[entry]->bbox)) {
                        enlargement = aux;
                        entry = i;
                    }
                }
            } else {
                /* we have found an entry that do not require any expansion!*/
                if (DB_IS_ZERO(aux)) {
                    enlargement = aux;
                    entry = i;
                    didfit = true;
                } else {
                    /* otherwise we add it into our array in order to check the overlapping area
                     ONLY if this node points to a leaf node*/
                    if (0 == height - 1) {
                        en[i].value = aux;
                        en[i].entry = i;
                    }
                    /*we compute it for other internal nodes that do not point to leaf nodes!*/
                    if (aux < enlargement) {
                        enlargement = aux; //we update the least enlargement
                        entry = i;
                    }

                }
            }
        }

#ifdef COLLECT_STATISTICAL_DATA
        _processed_entries_num += n->nofentries;
#endif

        /*we have to consider the overlapping area only if it points to leaf nodes!
         and we have not found enlargements equal to 0 before
         this refers to [determine the minimum overlap cost]
         which can be modified to reduce the cpu cost*/
        if (!didfit && 0 == height - 1) {
            /*[determine the nearly minimum overlap cost]*/
            int maxem, k;
            double leastoverlap, overlap;
            BBox *un;

            leastoverlap = DBL_MAX;

            /*Sort the rectangles in N in ascendent order of
            their area enlargement needed to include the new
            data rectangle*/
            qsort(en, n->nofentries, sizeof (Pair), double_asc_comp);

            /*Let A be the group of the first p entries*/
            if (n->nofentries < rstar->spec->max_neighbors_to_examine)
                maxem = n->nofentries;
            else
                maxem = rstar->spec->max_neighbors_to_examine;

            /*From the entries in A, considering all entries in N, 
             * choose the entry whose rectangle needs least
             * overlap enlargement Resolve ties as described above*/
            /*note that the ties were resolved before in this algorithm!*/
            for (i = 0; i < maxem; i++) {
                overlap = 0.0;
                un = bbox_union(n->entries[en[i].entry]->bbox, input->bbox);

                for (k = 0; k < n->nofentries; k++) {
                    if (k != i) {
#ifdef COLLECT_STATISTICAL_DATA
                        _processed_entries_num++;
#endif
                        if (bbox_check_predicate(un, n->entries[en[k].entry]->bbox, INTERSECTS)) {
                            overlap += bbox_overlap_area(un, n->entries[en[k].entry]->bbox);
                            if (bbox_check_predicate(n->entries[en[i].entry]->bbox, n->entries[en[k].entry]->bbox, INTERSECTS)) {
                                overlap -= bbox_overlap_area(n->entries[en[i].entry]->bbox, n->entries[en[k].entry]->bbox);
                            }
                        }
                    }
                }
                lwfree(un);

                if (leastoverlap > overlap) {
                    leastoverlap = overlap;
                    entry = en[i].entry;
                }
            }
        }
        //we free the pairs
        if (0 == height - 1) {
            lwfree(en);
        }

        //OK we have chosen the better path, 
        //then we put it in our stack to adjust this node after
        rnode_stack_push(stack, n, *chosen_address, entry);

        /*CS3 Set N to be the childnode pointed to by the
        childpointer of the chosen entry and repeat from CS2*/
        *chosen_address = n->entries[entry]->pointer;

        //        _DEBUG(NOTICE, "found a good entry");

        if (rstar->type == CONVENTIONAL_RSTARTREE)
            n = get_rnode(&rstar->base, n->entries[entry]->pointer, height - 1);
        else if (rstar->type == FAST_RSTARTREE_TYPE)
            n = (RNode *) fb_retrieve_node(&rstar->base, n->entries[entry]->pointer, height - 1);
        else if (rstar->type == eFIND_RSTARTREE_TYPE)
            n = (RNode *) efind_buf_retrieve_node(&rstar->base, efind_spc, n->entries[entry]->pointer, height - 1);
        else
            _DEBUGF(ERROR, "Invalid R*-tree specification %d", rstar->type);

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
        height--;
    }
    _DEBUG(ERROR, "Oops, no node was chosen in choose_node.");
    return NULL;
}

void adjust_rstartree(RStarTree *rstar, RNode *chosen_node, int cn_height, RNodeStack *stack) {
    BBox *n_bbox;

    int parent_add;
    int entry;
    int h = cn_height;

    RNode *n;

    bool adjusting = true;

    /*AT1 [Initialize.] Set N=L  */
    n = rnode_clone(chosen_node);
    //current_node will be the parent of n
    rnode_free(rstar->current_node);
    rstar->current_node = NULL;

    /*AT2 [Check If done ] If N is the root, stop*/
    while (h != rstar->info->height) {
        /*AT3 [Adjust covering rectangle in parent entry] Let P be the parent node of
N, and let EN be N’s entry in P Adjust ENI so that it tightly encloses
all entry rectangles in N.*/
        /*if we are not adjusting the tree anymore because it is not more necessary
         we stop!*/
        if (!adjusting)
            break;
        rstar->current_node = rnode_stack_pop(stack, &parent_add, &entry);
        n_bbox = rnode_compute_bbox(n);

        //we check if it is necessary to modify the BBOX of this parent
        if (!bbox_check_predicate(n_bbox, rstar->current_node->entries[entry]->bbox, EQUAL)) {
            memcpy(rstar->current_node->entries[entry]->bbox, n_bbox, sizeof (BBox));

            if (rstar->type == CONVENTIONAL_RSTARTREE) {
                put_rnode(&rstar->base, rstar->current_node, parent_add, h + 1);
            } else if (rstar->type == FAST_RSTARTREE_TYPE) {
                fb_put_mod_bbox(&rstar->base, fast_spc, parent_add, bbox_clone(n_bbox), entry, h + 1);
            } else if (rstar->type == eFIND_RSTARTREE_TYPE) {
                efind_buf_mod_node(&rstar->base, efind_spc, parent_add,
                        (void *) rentry_clone(rstar->current_node->entries[entry]), h + 1);
            } else {
                _DEBUGF(ERROR, "Invalid R*-tree specification %d", rstar->type);
            }

#ifdef COLLECT_STATISTICAL_DATA
            _written_int_node_num++;
            insert_writes_per_height(h + 1, 1);
#endif
            adjusting = true;

            /*AT5 [Move up to next level.] Set N=P. Repeat from AT2.*/
            rnode_free(n);
            n = rstar->current_node;
            rstar->current_node = NULL;
        } else {
            adjusting = false;
        }
        lwfree(n_bbox);

        h++;
    }
    /*we stopped to adjusted the tree, we set the current_node as the root node here*/
    if (!adjusting) {
        while (stack->size > 0) {
            rnode_free(rstar->current_node);
            rstar->current_node = rnode_stack_pop(stack, &parent_add, &entry);
        }
        rnode_free(n);
    } else {
        //we update the root node
        rstar->current_node = n;
    }
}

void reinsert_rstartree(RStarTree *rstar, RNode *chosen_node, int chosen_address, int cn_height, RNodeStack *stack) {
    BBoxCenter *allcenter;
    BBoxCenter *center;
    RNode *parent;
    int entry;
    int i, p;
    REntry **toreinsert;
    RNode *new;
    Pair *distances = (Pair*) lwalloc(sizeof (Pair) * chosen_node->nofentries);

#ifdef COLLECT_STATISTICAL_DATA
    _reinsertion_num++;
#endif

    /*RI1 For all M+1 entries of a node N, compute the distance
between the centers of their rectangles and the center
of the bounding rectangle of N*/
    //_DEBUG(NOTICE, "computing the centers");

    parent = rnode_stack_peek(stack, NULL, &entry);
    allcenter = bbox_get_center(parent->entries[entry]->bbox);
    for (i = 0; i < chosen_node->nofentries; i++) {
        center = bbox_get_center(chosen_node->entries[i]->bbox);
        distances[i].entry = i;
        distances[i].value = bbox_distance_between_centers(allcenter, center);
        lwfree(center);
    }
    lwfree(allcenter);

#ifdef COLLECT_STATISTICAL_DATA
    _processed_entries_num += chosen_node->nofentries;
#endif

    //_DEBUG(NOTICE, "computing the centers");

    /*RI2 Sort the entries in decreasing order of their distances
computed in RI1*/
    qsort(distances, chosen_node->nofentries, sizeof (Pair), double_desc_comp);

    /*RI3 Remove the first p entries from N and adjust the
bounding rectangle of N*/
    if (cn_height == 0) {
        p = chosen_node->nofentries * (rstar->spec->reinsert_perc_leaf_node / 100);
    } else {
        p = chosen_node->nofentries * (rstar->spec->reinsert_perc_internal_node / 100);
    }

    new = rnode_create_empty();
    toreinsert = (REntry**) lwalloc(sizeof (REntry*) * p);
    for (i = 0; i < p; i++) {
        toreinsert[i] = rentry_clone(chosen_node->entries[distances[i].entry]);
    }

    for (i = p; i < chosen_node->nofentries; i++) {
        rnode_add_rentry(new, rentry_clone(chosen_node->entries[distances[i].entry]));
    }

    if (rstar->type == CONVENTIONAL_RSTARTREE) {
        /*then we modify the chosen_node since we removed p elements from it*/
        put_rnode(&rstar->base, new, chosen_address, cn_height);
    } else if (rstar->type == FAST_RSTARTREE_TYPE) {
        /* the strategy is:
         * since we removed p entries from chosen_node, we created a node called new
         * for the FAST, we iterate the entries of new in order to modify the different entries in the buffer
         * we remove the remaining entries of chosen_node considering the size of new
         * as a result, we have stored in the buffer the node new.
         */
        int in;
        for (in = 0; in < new->nofentries; in++) {
            if (new->entries[in]->pointer != chosen_node->entries[in]->pointer) {
                //we modify the pointer and the bbox
                fb_put_mod_pointer(&rstar->base, fast_spc, chosen_address, new->entries[in]->pointer, in, cn_height);
                fb_put_mod_bbox(&rstar->base, fast_spc, chosen_address, bbox_clone(new->entries[in]->bbox), in, cn_height);
            }
        }
        //we need to 'del' the remaining entries in the reverse order
        //note the -2, because the real number of entries in the buffer was chosen_node->nofentries -1
        for (in = chosen_node->nofentries - 2; in >= new->nofentries; in--) {
            fb_put_mod_bbox(&rstar->base, fast_spc, chosen_address, NULL, in, cn_height);
        }
    } else if (rstar->type == eFIND_RSTARTREE_TYPE) {
        int in;
        /* the strategy is:
         * we remove this node
         * then we create it again
         * finally we insert this node with the non-removed elements
         */
        efind_buf_del_node(&rstar->base, efind_spc, chosen_address, cn_height);
        efind_buf_create_node(&rstar->base, efind_spc, chosen_address, cn_height);
        for (in = 0; in < new->nofentries; in++) {
            efind_buf_mod_node(&rstar->base, efind_spc, chosen_address,
                    (void *) rentry_clone(new->entries[in]), cn_height);
        }
    }

#ifdef COLLECT_STATISTICAL_DATA
    if (cn_height != 0)
        _written_int_node_num++;
    else
        _written_leaf_node_num++;
    insert_writes_per_height(cn_height, 1);
#endif

    /*then we adjust the tree since we change the node whose we removed p elements*/
    adjust_rstartree(rstar, new, cn_height, stack);
    rnode_free(new);

    //_DEBUG(NOTICE, "preparing to reinsert");

    /*RI4 In the sort, defined in R12, starting with the maximum
    distance (= far reinsert) or minimum distance (= close
    reinsert), invoke Insert to reinsert the entries*/
    if (rstar->spec->reinsert_type == FAR_REINSERT) {
        for (i = 0; i < p; i++) {
            insert_entry_rstartree(rstar, toreinsert[i], cn_height);
        }
    } else {
        for (i = p - 1; i >= 0; i--) {
            insert_entry_rstartree(rstar, toreinsert[i], cn_height);
        }
    }
    lwfree(distances);
    lwfree(toreinsert);
}

void insert_entry_rstartree(RStarTree *rstar, REntry *input, int i_height) {
    RNode *chosen_node; //the node in which was chosen to insert the input
    RNode *parent;
    int p_entry;
    int chosen_address; //page number of the chosen_node

    int max_entries;
    int p = 0;

    RNodeStack *stack = rnode_stack_init(); //to adjust_Tree   
    
    //_DEBUGF(NOTICE, "Inserting the entry %d", input->pointer);

    /*I1 Invoke ChooseSubtree. with the level as a parameter,
    to find an appropriate node N, in which to place the
    new entry E*/
    chosen_node = choose_node_rstartree(rstar, input, i_height, stack, &chosen_address);

    while (true) {
        if (i_height == 0) {
            max_entries = rstar->spec->max_entries_leaf_node;
            p = (max_entries + 1) * (rstar->spec->reinsert_perc_leaf_node / 100);
        } else {
            max_entries = rstar->spec->max_entries_int_node;
            p = (max_entries + 1) * (rstar->spec->reinsert_perc_internal_node / 100);
        }

        //_DEBUGF(NOTICE, "number of entries in %d is %d and the maximum number is %d", chosen_address, 
        //        chosen_node->nofentries, max_entries);

        /*I2 If N has less than M entries, accommodate E in N
        If N has M entries. invoke OverflowTreatment with the
        level of N as a parameter [for reinsertion or split]*/
        if (chosen_node->nofentries < max_entries) {
            rnode_add_rentry(chosen_node, input);
            /*then we can write the node with the new entry*/
            if (rstar->type == CONVENTIONAL_RSTARTREE) {
                put_rnode(&rstar->base, chosen_node, chosen_address, i_height);
            } else if (rstar->type == FAST_RSTARTREE_TYPE) {
                fb_put_mod_pointer(&rstar->base, fast_spc, chosen_address, input->pointer, chosen_node->nofentries - 1, i_height);
                fb_put_mod_bbox(&rstar->base, fast_spc, chosen_address, bbox_clone(input->bbox), chosen_node->nofentries - 1, i_height);
            } else if (rstar->type == eFIND_RSTARTREE_TYPE) {
                efind_buf_mod_node(&rstar->base, efind_spc, chosen_address,
                        (void*) rentry_clone(input), i_height);
            } else {
                _DEBUGF(ERROR, "Invalid R*-tree specification %d", rstar->type);
            }

#ifdef COLLECT_STATISTICAL_DATA
            if (i_height != 0)
                _written_int_node_num++;
            else
                _written_leaf_node_num++;
            insert_writes_per_height(i_height, 1);
#endif            
            /*I4 Adjust all covering rectangles in the insertion path
            such that they are minimum bounding boxes
            enclosing then children rectangles*/

            /*it can be called even in a reinsertion or split operation...*/
            //_DEBUG(NOTICE, "Adjusting the tree");
            adjust_rstartree(rstar, chosen_node, i_height, stack);
            //_DEBUG(NOTICE, "adjusted");
            rnode_free(chosen_node);
            rnode_stack_destroy(stack);
            break;
        }

        /* direct insert fails*/

        /*OverflowTreatment inlined here!!*/

        /*I3 If OverflowTreatment was called and a split was
        performed, propagate OverflowTreatment upwards if necessary
        If OverflowTreatment caused a split of the root, create a
        new root
         in addition, we only execute this if the number of p is higher than 0
         which means that we really have the possibility to reinsert p entries*/
        if (rstar->reinsert[i_height] && i_height != rstar->info->height && p > 0) {
            /*OT1 If the level is not the root level and this is the first
            call of OverflowTreatment in the given level
            during the Insertion of one data rectangle, then
            invoke Reinsert*/

            rstar->reinsert[i_height] = false;
            //we add the input in the chosen_node in order to make the force reinsert 
            //in this node with overcapacity
            rnode_add_rentry(chosen_node, input);
            //_DEBUG(NOTICE, "Processing reinsertion...");
            reinsert_rstartree(rstar, chosen_node, chosen_address, i_height, stack);
            //_DEBUG(NOTICE, "Done");
            rnode_free(chosen_node);
            rnode_stack_destroy(stack);
            break;
        } else {
            RNode *l, *ll;
           // RNode *cp = NULL;
            int split_address;
            l = rnode_create_empty();
            ll = rnode_create_empty();
            //we add the new entry in the current chosen_node
            rnode_add_rentry(chosen_node, input);

            //if (rstar->type == FAST_RSTARTREE_TYPE)
            //    cp = rnode_clone(chosen_node);

            //_DEBUG(NOTICE, "SPLITTING");
            //rnode_print(chosen_node, chosen_address);

            /*else invoke Split end*/
            rstartree_split_node(rstar->spec, chosen_node, i_height, l, ll);
            rnode_free(chosen_node);
            input = NULL;
            chosen_node = NULL;

            split_address = rtreesinfo_get_valid_page(rstar->info);

            if (rstar->type == CONVENTIONAL_RSTARTREE) {
                //we have to update the content of the chosen_node to l
                put_rnode(&rstar->base, l, chosen_address, i_height);
                //we have to write the new created node from the split            
                put_rnode(&rstar->base, ll, split_address, i_height);
            } else if (rstar->type == FAST_RSTARTREE_TYPE) {
                /*int in;
                for (in = 0; in < l->nofentries; in++) {
                    if (l->entries[in]->pointer != cp->entries[in]->pointer) {
                        //we put the new pointer and new bbox for the split node
                        fb_put_mod_pointer(&rstar->base, fast_spc, chosen_address, l->entries[in]->pointer, in, i_height);
                        fb_put_mod_bbox(&rstar->base, fast_spc, chosen_address, bbox_clone(l->entries[in]->bbox), in, i_height);
                    }
                }
                //we need to 'del' the remaining entries in the reverse order
                for (in = cp->nofentries - 2; in >= l->nofentries; in--) {
                    fb_put_mod_bbox(&rstar->base, fast_spc, chosen_address, NULL, in, i_height);
                }
                //we put the new node in the buffer
                fb_put_new_node(&rstar->base, fast_spc, split_address,
                        (void *) rnode_clone(ll), i_height);
                rnode_free(cp);
                 the methodo above is not efficient
                 */
                
                fb_del_node(&rstar->base, fast_spc, chosen_address, i_height);
                fb_put_new_node(&rstar->base, fast_spc, chosen_address, 
                        (void *) rnode_clone(l), i_height);
                
                fb_put_new_node(&rstar->base, fast_spc, split_address,
                        (void *) rnode_clone(ll), i_height);
            } else if (rstar->type == eFIND_RSTARTREE_TYPE) {
                int in;
                /* eFIND do this as follows: 
                 (i) we del the split node and store its pointer;
                 * (ii) we create it (again) in the buffer;
                 (iii) we add back the new entries
                 the advantage of this strategy is that we will not store NULL values in the buffer
                 * further, it will remove any previous modification of this node
                 consequently we will improve the buffer space utilization*/
                efind_buf_del_node(&rstar->base, efind_spc, chosen_address, i_height);
                efind_buf_create_node(&rstar->base, efind_spc, chosen_address, i_height);
                for (in = 0; in < l->nofentries; in++) {
                    efind_buf_mod_node(&rstar->base, efind_spc, chosen_address,
                            (void *) rentry_clone(l->entries[in]), i_height);
                }

                //we put the newly created node
                efind_buf_create_node(&rstar->base, efind_spc, split_address, i_height);
                for (in = 0; in < ll->nofentries; in++) {
                    efind_buf_mod_node(&rstar->base, efind_spc, split_address,
                            (void *) rentry_clone(ll->entries[in]), i_height);
                }
            }

#ifdef COLLECT_STATISTICAL_DATA
            if (i_height != 0)
                _written_int_node_num += 2;
            else
                _written_leaf_node_num += 2;
            insert_writes_per_height(i_height, 2);
#endif

            //_DEBUG(NOTICE, "SPLITTING done");

            if (i_height == rstar->info->height) {
                /*the split occurred in the root node
                 * then we must create a new root */
                RNode *new_root = rnode_create_empty();
                int new_root_add;
                
                //_DEBUG(NOTICE, "Creating new root node");

                rstar->reinsert = (bool*) lwrealloc(rstar->reinsert, sizeof (bool)*(i_height + 2));

                rstar->reinsert[i_height] = true;
                rstar->reinsert[i_height + 1] = false;

                //we allocate one more page for the new root
                new_root_add = rtreesinfo_get_valid_page(rstar->info);

                //the height of the tree is incremented
                rstar->info->height++;

                //the first entry of our new root is the old root node
                rnode_add_rentry(new_root, rentry_create(rstar->info->root_page, rnode_compute_bbox(l)));

                if (rstar->type == FAST_RSTARTREE_TYPE) {
                    //we put the new node in the buffer
                    fb_put_new_node(&rstar->base, fast_spc, new_root_add,
                            (void *) rnode_clone(new_root), rstar->info->height);
                } else if (rstar->type == eFIND_RSTARTREE_TYPE) {
                    //we firstly set the new height of the tree
                    if (efind_spc->read_buffer_policy == eFIND_HLRU_RBP)
                        efind_readbuffer_hlru_set_tree_height(rstar->info->height);

                    efind_buf_create_node(&rstar->base, efind_spc, new_root_add,
                            rstar->info->height);
                    efind_buf_mod_node(&rstar->base, efind_spc, new_root_add,
                            (void *) rentry_create(rstar->info->root_page, rnode_compute_bbox(l)),
                            rstar->info->height);
                }
                storage_update_tree_height(&rstar->base, rstar->info->height);

                //we update the root page
                rstar->info->root_page = new_root_add;

                //now the chosen_node is the new root
                chosen_node = new_root;
                chosen_address = new_root_add;
                /*the second entry of our new root is the split node
                 which will be inserted in the next iteration of this while
                 */
                input = rentry_create(split_address, rnode_compute_bbox(ll));
                //_DEBUGF(NOTICE, "New root node created with id %d", chosen_address);
            } else {
                BBox *l_bbox;
                //_DEBUG(NOTICE, "Checking if the parent should be modified");
                p_entry = 0;
                parent = rnode_stack_pop(stack, &chosen_address, &p_entry);
                l_bbox = rnode_compute_bbox(l);
                /*we check if the entry of parent that corresponds to l need to be updated*/
                if (!bbox_check_predicate(l_bbox, parent->entries[p_entry]->bbox, EQUAL)) {
                    memcpy(parent->entries[p_entry]->bbox, l_bbox, sizeof (BBox));
                    //_DEBUG(NOTICE, "Yes, it should");
                    //we only update the bbox for FAST e eFIND. We do not need to update in other cases because the parent will be written in the next iteration
                    if (rstar->type == FAST_RSTARTREE_TYPE) {
                        fb_put_mod_bbox(&rstar->base, fast_spc, chosen_address, bbox_clone(l_bbox), p_entry, i_height + 1);
                    } else if (rstar->type == eFIND_RSTARTREE_TYPE) {
                        efind_buf_mod_node(&rstar->base, efind_spc, chosen_address, (void*) rentry_clone(parent->entries[p_entry]), i_height + 1);
                    }
                }
                lwfree(l_bbox);

                /*now the input is the split node*/
                input = rentry_create(split_address, rnode_compute_bbox(ll));
                /*now the chosen_node will be the parent*/
                chosen_node = parent;
            }
            rnode_free(l);
            rnode_free(ll);

            //we move up one level in the tree
            i_height++;
            //_DEBUG(NOTICE, "Moving up one level in the tree");
        }
    }

}

RTree *rstartree_to_rtree(RStarTree *rstar) {
    RTree *r;
    SpatialIndex *si_rtree;

    //we create an empty r-tree
    si_rtree = rtree_empty_create(rstar->base.index_file, rstar->base.src,
            rstar->base.gp, rstar->base.bs, false);
    r = (void *) si_rtree;

    //now we set the rstar tree into this index
    //type
    if (rstar->type == CONVENTIONAL_RSTARTREE)
        r->type = CONVENTIONAL_RTREE;
    else if (rstar->type == FAST_RSTARTREE_TYPE)
        r->type = FAST_RTREE_TYPE;
    else if (rstar->type == eFIND_RSTARTREE_TYPE) {
        r->type = eFIND_RTREE_TYPE;
    } else {
        _DEBUGF(ERROR, "Invalid R*-tree specification %d", rstar->type);
    }
    //current_node
    r->current_node = rnode_clone(rstar->current_node);
    //info
    rtreesinfo_free(r->info);
    r->info = rstar->info;
    //spec
    r->spec->max_entries_int_node = rstar->spec->max_entries_int_node;
    r->spec->max_entries_leaf_node = rstar->spec->max_entries_leaf_node;
    r->spec->min_entries_int_node = rstar->spec->min_entries_int_node;
    r->spec->min_entries_leaf_node = rstar->spec->min_entries_leaf_node;
    r->spec->split_type = RSTARTREE_SPLIT;

    return r;
}

void free_converted_rtree(RTree *rtree) {
    rnode_free(rtree->current_node);
    rtree->current_node = NULL;
    lwfree(rtree->spec);
    lwfree(rtree);
}

bool delete_entry_rstartree(RStarTree *rstar, const REntry *to_remove) {
    RTree *r;
    RNodeStack *removed_nodes;
    bool ret;
    int i;
    RNode *n;
    int level;

    //we first convert the rstartree to an rtree since this is the same deletion algorithm
    r = rstartree_to_rtree(rstar);

    if (rstar->type == FAST_RSTARTREE_TYPE)
        rtree_set_fastspecification(fast_spc);
    else if (rstar->type == eFIND_RSTARTREE_TYPE)
        rtree_set_efindspecification(efind_spc);

    removed_nodes = rnode_stack_init();
    //we will handle the removed_nodes!
    ret = rtree_remove_with_removed_nodes(r, to_remove, removed_nodes, false);

    /*next we update our current node of the r*-tree*/
    rnode_free(rstar->current_node);
    rstar->current_node = NULL;
    rstar->current_node = rnode_clone(r->current_node);

    while (removed_nodes->size > 0) {
        n = rnode_stack_pop(removed_nodes, &level, NULL);
        for (i = 0; i < n->nofentries; i++) {
            insert_entry_rstartree(rstar, rentry_clone(n->entries[i]), level);
        }
        rnode_free(n);
    }

    /*D4 [Shorten tree.] If the root node has only one child after the tree has
been adjusted, make the child the new root*/
    if (rstar->current_node->nofentries == 1 && rstar->info->height > 0) {
        int p;
        RNode *new_root = NULL;
        p = rstar->current_node->entries[0]->pointer;

        /*remove from the disk*/
        if (rstar->type == CONVENTIONAL_RSTARTREE) {
            del_rnode(&rstar->base, rstar->info->root_page, rstar->info->height);
        } else if (rstar->type == FAST_RSTARTREE_TYPE) {
            fb_del_node(&rstar->base, fast_spc, rstar->info->root_page, rstar->info->height);
        } else if (rstar->type == eFIND_RSTARTREE_TYPE) {
            //we update the height of three
            if (efind_spc->read_buffer_policy == eFIND_HLRU_RBP)
                efind_readbuffer_hlru_set_tree_height(rstar->info->height - 1);
            efind_buf_del_node(&rstar->base, efind_spc, rstar->info->root_page, rstar->info->height);
        } else {
            _DEBUGF(ERROR, "Invalid R*-tree specification %d", rstar->type);
        }
        storage_update_tree_height(&rstar->base, rstar->info->height - 1);
        
        //we add the removed page as an empty page now
        rtreesinfo_add_empty_page(rstar->info, rstar->info->root_page);

#ifdef COLLECT_STATISTICAL_DATA
        _deleted_int_node_num++;
        insert_writes_per_height(rstar->info->height, 1);
#endif

        /* from now on, we modify the RSTARTREE*/
        rnode_free(rstar->current_node);
        rstar->info->root_page = p;

        if (rstar->type == CONVENTIONAL_RSTARTREE)
            new_root = get_rnode(&rstar->base, p, rstar->info->height - 1);
        else if (rstar->type == FAST_RSTARTREE_TYPE)
            new_root = (RNode *) fb_retrieve_node(&rstar->base, p, rstar->info->height - 1);
        else if (rstar->type == eFIND_RSTARTREE_TYPE)
            new_root = (RNode *) efind_buf_retrieve_node(&rstar->base, efind_spc, p, rstar->info->height - 1);

        rstar->current_node = new_root;
        rstar->info->height--;

#ifdef COLLECT_STATISTICAL_DATA
        if (rstar->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(rstar->info->height, 1);
#endif        
    }

    /*freeing memory*/
    rnode_stack_destroy(removed_nodes);
    free_converted_rtree(r);

    return ret;
}

/*********************************
 * functions in order to make RStarTree a standard SpatialIndex (see spatial_index.h)
 *********************************/
static uint8_t rstartree_get_type(const SpatialIndex *si) {
    RStarTree *rstar = (void *) si;
    return rstar->type;
}

/* original insert algorithm from R*-tree
 * It uses the choose_node and adjust_tree, which are static functions in rstartree.c
 * These subfunctions use split algorithms
 * it inserts a new entry into a LEAF node!
 */
static bool rstartree_insert(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    BBox *bbox = (BBox*) lwalloc(sizeof (BBox));
    RStarTree *rstar = (void *) si;
    REntry *input;
    int i;

    gbox_to_bbox(geom->bbox, bbox);

    input = rentry_create(pointer, bbox);

    /*Algorithm InsertData
ID1 Invoke Insert starting with the leaf level as a
parameter, to Insert a new data rectangle*/
    insert_entry_rstartree(rstar, input, 0);
    /*we reset the reinsert to true for all levels*/
    for (i = 0; i < rstar->info->height; i++) {
        rstar->reinsert[i] = true;
    }

    return true;
}

static bool rstartree_remove(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    BBox *bbox = (BBox*) lwalloc(sizeof (BBox));
    RStarTree *rstar = (void *) si;
    REntry *rem;
    bool ret;

    gbox_to_bbox(geom->bbox, bbox);
    rem = rentry_create(pointer, bbox);

    ret = delete_entry_rstartree(rstar, rem);

    lwfree(rem->bbox);
    lwfree(rem);
    return ret;
}

static bool rstartree_update(SpatialIndex *si, int oldpointer, const LWGEOM *oldgeom, int newpointer, const LWGEOM *newgeom) {
    bool r, i = false;
    //TO-DO improve the return value of this.. i.e., it can return an error identifier.
    r = rstartree_remove(si, oldpointer, oldgeom);
    if (r)
        i = rstartree_insert(si, newpointer, newgeom);
    return r && i;
}

static SpatialIndexResult *rstartree_search_ss(SpatialIndex *si, const LWGEOM *search_object, uint8_t predicate) {
    SpatialIndexResult *sir;
    BBox *search = (BBox*) lwalloc(sizeof (BBox));
    RStarTree *rstar = (void *) si;
    RTree *r;

    gbox_to_bbox(search_object->bbox, search);

    //we first convert the rstartree to an rtree since this is the same search algorithm
    r = rstartree_to_rtree(rstar);

    if (rstar->type == FAST_RSTARTREE_TYPE)
        rtree_set_fastspecification(fast_spc);
    else if (rstar->type == eFIND_RSTARTREE_TYPE)
        rtree_set_efindspecification(efind_spc);
    sir = rtree_search(r, search, predicate);

    free_converted_rtree(r);
    lwfree(search);
    return sir;
}

static bool rstartree_header_writer(SpatialIndex *si, const char *file) {
    festival_header_writer(file, CONVENTIONAL_RSTARTREE, si);
    return true;
}

static void rstartree_destroy(SpatialIndex *si) {
    RStarTree *rstar = (void *) si;
    rnode_free(rstar->current_node);
    lwfree(rstar->spec);
    rtreesinfo_free(rstar->info);
    lwfree(rstar->reinsert);

    generic_parameters_free(rstar->base.gp);
    source_free(rstar->base.src);
    lwfree(rstar->base.index_file);

    lwfree(rstar);
}

/* return a new R*-tree index, it also specifies the RSTARTREE_SPECIFICATION*/
SpatialIndex *rstartree_empty_create(char *file, Source *src, GenericParameters *gp,
        BufferSpecification *bs, bool persist) {
    RStarTree *rstar;

    /*define the general functions of the rstartree*/
    static const SpatialIndexInterface vtable = {rstartree_get_type,
        rstartree_insert, rstartree_remove, rstartree_update, rstartree_search_ss,
        rstartree_header_writer, rstartree_destroy};
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    rstar = (RStarTree*) lwalloc(sizeof (RStarTree));
    memcpy(&rstar->base, &base, sizeof (base));
    rstar->type = CONVENTIONAL_RSTARTREE; //this is a conventional r*-tree

    rstar->spec = (RStarTreeSpecification*) lwalloc(sizeof (RStarTreeSpecification));
    rstar->info = rtreesinfo_create(0, 0, 0);
    rstar->reinsert = (bool*) lwalloc(sizeof (bool)*1);
    rstar->reinsert[0] = true;
    rstar->current_node = NULL;

    //we have to persist the empty node
    if (persist) {
        rstar->current_node = rnode_create_empty();
        put_rnode(&rstar->base, rstar->current_node,
                rstar->info->root_page, rstar->info->height);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &rstar->base;
}
