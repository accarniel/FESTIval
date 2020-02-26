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

/* This file implements the Hilbert R-tree index according to the original article
 * Reference: KAMEL, I.; FALOUTSOS, C. Hilbert R-tree: An Improved R-tree Using Fractals. 
 * Proceedings of the VLDB Conference, p. 500-509, 1994.
 */

#include "hilbertrtree.h"

#include "../main/log_messages.h" //messages errors
#include "../fast/fast_buffer.h" //for FAST indices
#include "../efind/efind_buffer_manager.h" //for eFIND indices
#include "../efind/efind_read_buffer_policies.h"

#include "../main/header_handler.h" //for header storage
#include "../main/storage_handler.h" //for the tree height update

#include "../main/statistical_processing.h"
#include "hilbertnode_stack.h" // in order to collect statistical data

//the possible cases of an insertion/remotion
#define HILBERT_DIRECT       1
#define HILBERT_RED_WITH_MOD        2
#define HILBERT_RED_WITHOUT_MOD     3
#define HILBERT_SPLIT               4
#define HILBERT_MERGE           5        

/*we need this function in order to make this R-tree index "FASTable"
 that is, in order to be used as a FAST index*/
static FASTSpecification *fast_spc;

void hilbertrtree_set_fastspecification(FASTSpecification *fesp) {
    fast_spc = fesp;
}

/*we need this function in order to make this R-tree an eFIND R-tree index
 that is, in order to be used as a eFIND index*/
static eFINDSpecification *efind_spc;

void hilbertrtree_set_efindspecification(eFINDSpecification *fesp) {
    efind_spc = fesp;
}

/*recursive search for the r-tree, such that specified in the original R-tree paper.*/
static SpatialIndexResult *recursive_search(HilbertRTree *rtree, const BBox *query,
        uint8_t predicate, int height, SpatialIndexResult *result);
/*this function calls the recursive search, which is almost the same algorithm of the R-tree*/
static SpatialIndexResult *hilbertrtree_search(HilbertRTree *hrtree, const BBox *search, uint8_t predicate);
static bool insert_entry(HilbertRTree *hrtree, REntry *input);
static bool remove_entry(HilbertRTree *hrtree, REntry *rem);
/*this function implements the classical split 1-to-2 to be applied in the root node*/
static HilbertRNode *split1to2(const HilbertRNode *n, HilbertRNode *l);
/* this function receives the following parameters:
 * the HilbertRTree hrtree, which is used to write redistributed nodes
 *   (this function only writes the redistributed nodes. It does not write the split node)
 * node n, which is our overflowed node - it already the newly created entry!
 * n_add, the address of the node n
 * n_height, which corresponds to the height (in order to know if we are dealing with internal or leaf nodes)
 * entry_of_n_in_p,. the index of n in parent_n
 * parent_n, which provides the parent node of n in order to extract the cooperating siblings (s - 1 siblings)
 * flag, which provides the type of modifications that were performed
 * this function returns a newly created node (if any) or NULL
 * IMPORTANT NODES:
 * 1 - we do not free the n, since it is responsibility for the caller
 */
static HilbertRNode *handle_overflow(HilbertRTree *hrtree, HilbertRNode *n, int n_add, int n_height,
        int entry_of_n_in_p, HilbertRNode *parent_n, int parent_add, uint8_t *flag);
/*very similar to the handle_overflow, but this function handles the underflow cases
 return the entry to be removed from the parent node, and -1 otherwise*/
static int handle_underflow(HilbertRTree *hrtree, HilbertRNode *n, int n_add, int n_height,
        int entry_of_n_in_p, HilbertRNode *parent_n, int parent_add, uint8_t *flag);
static HilbertRNode *choose_node(HilbertRTree *hrtree, hilbert_value_t hilbert,
        int height, HilbertRNodeStack *stack, int *chosen_address);
/*function responsible to adjust the tree, called for insertion and remotion*/
static HilbertRNode *adjust_tree(HilbertRTree *hrtree, HilbertRNode *l, HilbertRNode *ll,
        int *split_address, int *removed_entry, int l_height, HilbertRNodeStack *stack, uint8_t flag);

SpatialIndexResult *recursive_search(HilbertRTree *hrtree, const BBox *query,
        uint8_t predicate, int height, SpatialIndexResult *result) {
    HilbertRNode *node;
    int i;
    uint8_t p;

    /* we copy the current node for backtracking purposes 
     that is, in order to follow several positive paths in the tree*/
    node = hilbertnode_clone(hrtree->current_node);

    /*internal node
     * let T = rtree->current_node, S = query
     S1 [Search subtrees] If T is not a leaf, check each entry E to determine
whether EI overlaps S. For all overlapping entries, invoke Search on the tree
whose root node is pointed to by Ep
     Note that we improve it by using the next comment.*/
    if (height != 0) {
        for (i = 0; i < hrtree->current_node->nofentries; i++) {
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

            /*it is only internal nodes*/
            if (bbox_check_predicate(query, hrtree->current_node->entries.internal[i]->bbox, p)) {
                //we get the node in which the entry points to
                if (hrtree->type == CONVENTIONAL_HILBERT_RTREE)
                    hrtree->current_node = get_hilbertnode(&hrtree->base,
                        hrtree->current_node->entries.internal[i]->pointer, height - 1);
                else if (hrtree->type == FAST_HILBERT_RTREE_TYPE)
                    hrtree->current_node = (HilbertRNode *) fb_retrieve_node(&hrtree->base,
                        hrtree->current_node->entries.internal[i]->pointer, height - 1);
                else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE)
                    hrtree->current_node = (HilbertRNode *) efind_buf_retrieve_node(&hrtree->base, efind_spc,
                        hrtree->current_node->entries.internal[i]->pointer, height - 1);
                else //it should not happen
                    _DEBUGF(ERROR, "Invalid Hilbert R-tree specification %d", hrtree->type);

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

                result = recursive_search(hrtree, query, predicate, height - 1, result);

                /*after to traverse this child, we need to back 
                 * the reference of the current_node for the original one */
                hilbertnode_copy(hrtree->current_node, node);
            }
        }
    }/* leaf node 
      S2 [Search leaf nodes] If T is a leaf, check all entries E to determine
whether EI overlaps S. If so, E is a qualifying record
      Note that we improve it by using the next comment.*/
    else {
        for (i = 0; i < hrtree->current_node->nofentries; i++) {

#ifdef COLLECT_STATISTICAL_DATA
            _processed_entries_num++;
#endif

            /*  * We employ MBRs relationships, like defined in: 
             * CLEMENTINI, E.; SHARMA, J.; EGENHOFER, M. J. Modelling topological spatial relations:
 Strategies for query processing. Computers & Graphics, v. 18, n. 6, p. 815–822, 1994.*/
            if (bbox_check_predicate(query, hrtree->current_node->entries.leaf[i]->bbox, predicate)) {
                spatial_index_result_add(result, hrtree->current_node->entries.leaf[i]->pointer);
            }
        }
    }
    hilbertnode_free(node);
    return result;
}

/*default searching algorithm of the Hilbert R-tree (defined in rtree.h)*/
SpatialIndexResult *hilbertrtree_search(HilbertRTree *hrtree, const BBox *search, uint8_t predicate) {
    SpatialIndexResult *sir = spatial_index_result_create();
    /* current node here MUST be equal to the root node */
    if (hrtree->current_node != NULL) {
        sir = recursive_search(hrtree, search, predicate, hrtree->info->height, sir);
    }
    return sir;
}

HilbertRNode *handle_overflow(HilbertRTree *hrtree, HilbertRNode *n, int n_add, int n_height,
        int entry_of_n_in_p, HilbertRNode *parent_n, int parent_add, uint8_t *flag) {
    /* return the new node if a split occurred. 
        H1. let E be a set that contains all the entries from N and its s - 1 cooperating siblings.
     * 
     **/
    HilbertRTreeSpecification *spec = hrtree->spec;
    //we will use the HilbertIEntry here since it has the hilbert value, but this does not mean that we will handle only internal nodes
    HilbertIEntry **entries; //this is our E
    HilbertRNode **s_nodes; //the S set
    int s_length; //number of nodes in s_nodes
    int nofentries = 0; //how many entries are stored in entries?
    int cur = 0; //the current entry from entries that is being inserted in the redistribution step
    int current_index; //the current index of parent_n that is being used as sibling
    int i, j; //auxiliary counters
    int max_entries_per_node;
    int nofentries_per_node = 0;
    int cur_pointer; //in the adjustment
    BBox *bbox_node, *bbox_entry;
    hilbert_value_t h;
    int left; //how many siblings should be considered for from the left direction
    int right;
    int start;
    HilbertRNode *nn = NULL; //split node, which is returned in this function
    uint8_t type = n->type;
    int osp = spec->order_splitting_policy;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec startt;
    struct timespec end;

    cpustart = get_CPU_time();
    startt = get_current_time();
#endif   

    if (parent_n == NULL && osp > 1) {
        _DEBUG(ERROR, "Handle overflow is being calling in a wrong way.");
    }

    if (n_height == 0)
        max_entries_per_node = spec->max_entries_leaf_node;
    else
        max_entries_per_node = spec->max_entries_int_node;

    /*we make an important checking
     we check if the parent node has enough child entries to satisfy the order splitting policy
     * if so, fine. Otherwise, we consider the number of child entries of the parent
     * this is done for handing order splitting policies greater than 3
     */
    if (osp > parent_n->nofentries) {
        osp = parent_n->nofentries;
    }

    entries = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * ((osp * max_entries_per_node) + 1));
    s_nodes = (HilbertRNode**) lwalloc(sizeof (HilbertRNode*) * osp);
    s_length = osp;

    //we divide by 2 since there are two sides, therefore, how many entries from the left side should be considered?
    //note that the preference is for the right side
    //note that we will consider s-1 cooperating siblings!
    left = (int) (osp - 1) / 2;
    right = (osp - 1) - left;

    if ((entry_of_n_in_p + right) >= parent_n->nofentries) {
        //check if we are not overflowing the number of elements in the parent in the right direction
        int dif = (entry_of_n_in_p + right) - (parent_n->nofentries - 1);
        left += dif;
    } else if (entry_of_n_in_p - left < 0) {
        //check if we are not overflowing the number of elements in the parent in the left direction
        int dif = (left - entry_of_n_in_p);
        left -= dif;
    }

    start = entry_of_n_in_p - left;

    current_index = start;

    //_DEBUGF(NOTICE, "Overflow handle variables -> nof of n (%d), max of entries (%d) "
    //        "and entry_of_n_in_p (%d)", n->nofentries, max_entries_per_node, entry_of_n_in_p);
    //_DEBUGF(NOTICE, "The start index is %d and the n node is", start);
    //hilbertnode_print(n, n_add);

    //_DEBUG(NOTICE, "cooperate siblings...");

    /* H2. add r to E
     in addition, we need to read from the storage device the s-1 cooperating siblings
     we respect the order by the LHV here*/
    for (i = 0; i < s_length; i++) {
        if (current_index != entry_of_n_in_p) {
            if (hrtree->type == CONVENTIONAL_HILBERT_RTREE)
                s_nodes[i] = get_hilbertnode(&hrtree->base, parent_n->entries.internal[current_index]->pointer, n_height);
            else if (hrtree->type == FAST_HILBERT_RTREE_TYPE)
                s_nodes[i] = (HilbertRNode*) fb_retrieve_node(&hrtree->base, parent_n->entries.internal[current_index]->pointer, n_height);
            else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE)
                s_nodes[i] = (HilbertRNode*) efind_buf_retrieve_node(&hrtree->base, efind_spc, parent_n->entries.internal[current_index]->pointer, n_height);

#ifdef COLLECT_STATISTICAL_DATA
            if (n_height > 0)
                _visited_int_node_num++;
            else
                _visited_leaf_node_num++;
            insert_reads_per_height(n_height, 1);
#endif
        } else {
            s_nodes[i] = n;
            n = NULL;
        }

        //hilbertnode_print(s_nodes[i], parent_n->entries.internal[current_index]->pointer);

        if (s_nodes[i]->type == HILBERT_INTERNAL_NODE) {
            for (j = 0; j < s_nodes[i]->nofentries; j++) {
                entries[nofentries] = s_nodes[i]->entries.internal[j];
                nofentries++;
            }
            //we free the list of entries but not the entries (they are in the entries)
            lwfree(s_nodes[i]->entries.internal);
        } else {
            for (j = 0; j < s_nodes[i]->nofentries; j++) {
                entries[nofentries] = hilbertentry_create(s_nodes[i]->entries.leaf[j]->pointer,
                        s_nodes[i]->entries.leaf[j]->bbox, 0);
                lwfree(s_nodes[i]->entries.leaf[j]);
                nofentries++;
            }
            //we free the list of entries but not the entries (they are in the entries)
            lwfree(s_nodes[i]->entries.leaf);
        }

        s_nodes[i]->nofentries = 0; //this is an empty node now

        current_index++;
    }

    //the number of nodes will vary according to the following cases:

    /* H3. if at least one of the s - 1 cooperating siblings is not full, distribute E
evenly among the s nodes according to the Hilbert value.
     */
    if (nofentries <= (osp * max_entries_per_node)) {
        //we will allow this number of entries per node in the redistribution
        nofentries_per_node = ceil((double) nofentries / (double) osp);
        if (nofentries_per_node > max_entries_per_node) {
            _DEBUG(ERROR, "Wow, the redistribution is not sufficient!");
        }
        *flag = HILBERT_RED_WITHOUT_MOD;
       // _DEBUGF(NOTICE, "Only a redistribution is needed. Each node will have: %d elements", nofentries_per_node);
    } else {
        /*
H4. if all the s cooperating siblings are full,
create a new node NN and
distribute & evenly among the s + 1 nodes
according to the Hilbert value.
return NN. */
        *flag = HILBERT_SPLIT;
        nofentries_per_node = ceil((double) nofentries / (double) (osp + 1));
        //since we have a split node, we need to create it (our nn)
        nn = hilbertnode_create_empty(type);

        //_DEBUGF(NOTICE, "All cooperating siblings are full, a splitting is needed. Each node will have %d entries", nofentries_per_node);
#ifdef COLLECT_STATISTICAL_DATA
        if (type == HILBERT_INTERNAL_NODE) {
            _split_int_num++;
        } else {
            _split_leaf_num++;
#endif
        }
        s_nodes = (HilbertRNode**) lwrealloc(s_nodes,
                (osp + 1) * sizeof (HilbertRNode*));
        s_nodes[osp] = nn;
        s_length++;
    }

#ifdef COLLECT_STATISTICAL_DATA
    //we did not count the new entry
    _processed_entries_num += (nofentries - 1);
#endif

    //_DEBUG(NOTICE, "Performing the redistribution in the s_nodes");
    /*we make the distribution here*/
    if (type == HILBERT_INTERNAL_NODE) {
        int last = s_length - 1;
        int size_last;
        for (i = 0; i < (s_length - 1); i++) {
            //we allow space for the entries
            s_nodes[i]->entries.internal = (HilbertIEntry**) lwalloc(nofentries_per_node * sizeof (HilbertIEntry*));

            //we now copy the reference from E
            for (j = 0; j < nofentries_per_node; j++) {
                s_nodes[i]->entries.internal[j] = entries[cur];
                cur++;
            }
            s_nodes[i]->nofentries = nofentries_per_node;
        }

        //we now put the remaining entries in the last node
        size_last = nofentries - cur;
        //we alloc space        
        s_nodes[last]->entries.internal = (HilbertIEntry**) lwalloc(size_last * sizeof (HilbertIEntry*));
        //we now copy the reference from E
        for (j = 0; j < size_last; j++) {
            s_nodes[last]->entries.internal[j] = entries[cur];
            cur++;
        }
        s_nodes[last]->nofentries = size_last;
    } else {
        int last = s_length - 1;
        int size_last;
        for (i = 0; i < (s_length - 1); i++) {
            //we alloc space
            s_nodes[i]->entries.leaf = (REntry**) lwalloc(nofentries_per_node * sizeof (REntry*));
            //we now copy the entry from E
            for (j = 0; j < nofentries_per_node; j++) {
                s_nodes[i]->entries.leaf[j] = rentry_create(entries[cur]->pointer, entries[cur]->bbox);
                lwfree(entries[cur]);
                cur++;
            }

            s_nodes[i]->nofentries = nofentries_per_node;
        }
        //if there are more entries, we put it into the last node
        //we now put the remaining entries in the last node
        size_last = nofentries - cur;
        //we check if the old space of s_nodes[i] is sufficient to hold the number of entries in the distribution
        s_nodes[last]->entries.leaf = (REntry**) lwalloc(size_last * sizeof (REntry*));
        //we now copy the reference from E
        for (j = 0; j < size_last; j++) {
            s_nodes[last]->entries.leaf[j] = rentry_create(entries[cur]->pointer, entries[cur]->bbox);
            lwfree(entries[cur]);
            cur++;
        }
        s_nodes[last]->nofentries = size_last;
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    //here we consider the split time as the handle_overflow time
    _split_cpu_time += get_elapsed_time(cpustart, cpuend);
    _split_time += get_elapsed_time(startt, end);
#endif  

    current_index = start;

    //_DEBUG(NOTICE, "after the redistribution, the cooperate siblings are...");

    //we then write the modified nodes, excepting the split node (if any)
    if (*flag == HILBERT_SPLIT)
        s_length--;
    for (i = 0; i < s_length; i++) {
        if (current_index == entry_of_n_in_p) {
            cur_pointer = n_add;
        } else {
            cur_pointer = parent_n->entries.internal[current_index]->pointer;
        }

        /*we must write the modified nodes!*/
        if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
            put_hilbertnode(&hrtree->base, s_nodes[i], cur_pointer, n_height);
        } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
            //we have to remove the node and then insert it
            fb_del_node(&hrtree->base, fast_spc, cur_pointer, n_height);
            fb_put_new_node(&hrtree->base, fast_spc, cur_pointer,
                    (void*) hilbertnode_clone(s_nodes[i]), n_height);
        } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
            efind_buf_del_node(&hrtree->base, efind_spc, cur_pointer, n_height);
            efind_buf_create_node(&hrtree->base, efind_spc, cur_pointer, n_height);
            if (type == HILBERT_INTERNAL_NODE) {
                for (j = 0; j < s_nodes[i]->nofentries; j++) {
                    efind_buf_mod_node(&hrtree->base, efind_spc, cur_pointer,
                            (void*) hilbertientry_clone(s_nodes[i]->entries.internal[j]), n_height);
                }
            } else {
                for (j = 0; j < s_nodes[i]->nofentries; j++) {
                    efind_buf_mod_node(&hrtree->base, efind_spc, cur_pointer,
                            (void*) rentry_clone(s_nodes[i]->entries.leaf[j]), n_height);
                }
            }
        } else
            _DEBUGF(ERROR, "Invalid Hilbert R-tree specification %d", hrtree->type);

        //hilbertnode_print(s_nodes[i], cur_pointer);

#ifdef COLLECT_STATISTICAL_DATA
        if (n_height > 0)
            _written_int_node_num++;
        else
            _written_leaf_node_num++;
        insert_writes_per_height(n_height, 1);
#endif

        //we adjust the MBR and/or LHV in the parent
        bbox_node = bbox_create();
        h = hilbertnode_compute_bbox(s_nodes[i], spec->srid, bbox_node);
        bbox_entry = parent_n->entries.internal[current_index]->bbox;
        if (!bbox_check_predicate(bbox_node, bbox_entry, EQUAL) || h != parent_n->entries.internal[current_index]->lhv) {
            //we should modify the bbox or lhv
            if (*flag == HILBERT_RED_WITHOUT_MOD)
                *flag = HILBERT_RED_WITH_MOD;

            if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                fb_put_mod_lhv(&hrtree->base, fast_spc, parent_add, h, current_index, n_height + 1);
                fb_put_mod_bbox(&hrtree->base, fast_spc, parent_add, bbox_clone(bbox_node), current_index, n_height + 1);
            }

            memcpy(parent_n->entries.internal[current_index]->bbox, bbox_node, sizeof (BBox));
            parent_n->entries.internal[current_index]->lhv = h;

            //we only apply the modifications for FAST e eFIND since they apply it into the buffer
            //after the return of this function, the caller should write the parent node
            if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                efind_buf_mod_node(&hrtree->base, efind_spc, parent_add,
                        (void*) hilbertientry_clone(parent_n->entries.internal[current_index]), n_height + 1);
            }
        }
        lwfree(bbox_node);

        //we cannot free the n, since it is responsibility for the caller
        if (current_index != entry_of_n_in_p) {
            hilbertnode_free(s_nodes[i]);
        } else {
            n = s_nodes[i];
        }

        // _DEBUG(NOTICE, "Adjusted");

        current_index++;
    }

    lwfree(s_nodes);
    lwfree(entries);

    //_DEBUG(NOTICE, "Handle overflow processed");

    return nn;
}

/*in contrast of the handle_overflow, this function considers s cooperating siblings
 * and return the entry of the parent that should be removed
 */
int handle_underflow(HilbertRTree *hrtree, HilbertRNode *n, int n_add, int n_height,
        int entry_of_n_in_p, HilbertRNode *parent_n, int parent_add, uint8_t *flag) {
    /* return the entry of the parent if an underflow was performed
        U1. let E be a set that contains all the entries from N and its s cooperating siblings.
     * 
     **/
    HilbertRTreeSpecification *spec = hrtree->spec;
    //we will use the HilbertIEntry here since it has the hilbert value, but this does not mean that we will handle only internal nodes
    HilbertIEntry **entries; //this is our E
    HilbertRNode **s_nodes; //the S set
    int s_length; //number of nodes in s_nodes
    int nofentries = 0; //how many entries are stored in entries?
    int cur = 0; //the current entry from entries that is being inserted in the redistribution step
    int current_index; //the current index of parent_n that is being used as sibling
    int i, j; //auxiliary counters
    int max_entries_per_node, min_entries_per_node;
    int nofentries_per_node = 0;
    int cur_pointer; //in the adjustment
    BBox *bbox_node, *bbox_entry;
    hilbert_value_t h;
    int left; //how many siblings should be considered for from the left direction
    int right;
    int start;
    int remove_this_entry = -1; //underflow, removed node, which is returned in this function
    uint8_t type = n->type;
    int osp = spec->order_splitting_policy;

    if (parent_n == NULL) {
        _DEBUG(ERROR, "Handle underflow is being calling for the root node, which is invalid.");
    }

    if (n_height == 0) {
        max_entries_per_node = spec->max_entries_leaf_node;
        min_entries_per_node = spec->min_entries_leaf_node;
    } else {
        max_entries_per_node = spec->max_entries_int_node;
        min_entries_per_node = spec->min_entries_int_node;
    }

    /*we make an important checking
     we check if the parent node has enough child entries to satisfy the order splitting policy
     * if so, fine. Otherwise, we consider the number of child entries of the parent
     * this is done when the parent node is the root node
     */
    if ((osp + 1) > parent_n->nofentries) {
        osp = parent_n->nofentries - 1;
    }

    //we consider order_splitting_policy+1 because we also should take into account the underflowed node
    entries = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * ((osp + 1) * max_entries_per_node));
    s_nodes = (HilbertRNode**) lwalloc(sizeof (HilbertRNode*) * (osp + 1));
    s_length = osp + 1;

    //we divide by 2 since there are two sides, therefore, how many entries from the left side should be considered?
    //note that the preference is for the right side
    left = (int) osp / 2;
    right = osp - left;

    if ((entry_of_n_in_p + right) >= parent_n->nofentries) {
        //check if we are not overflowing the number of elements in the parent in the right direction
        int dif = (entry_of_n_in_p + right) - (parent_n->nofentries - 1);
        left += dif;
    } else if (entry_of_n_in_p - left < 0) {
        //check if we are not overflowing the number of elements in the parent in the left direction
        int dif = (left - entry_of_n_in_p);
        left -= dif;
    }

    start = entry_of_n_in_p - left;

    current_index = start;


    //_DEBUGF(NOTICE, "Starting from the index %d", start);

    //_DEBUGF(NOTICE, "Underflow handle variables -> nof of n (%d), max of entries (%d) "
    //        "and entry_of_n_in_p (%d)", n->nofentries, min_entries_per_node, entry_of_n_in_p);

    //_DEBUG(NOTICE, "Retrieving the nodes from the storage device");

    /* we need to read the s cooperating sibling nodes from the storage device
     we respect the order by the LHV here*/
    for (i = 0; i < s_length; i++) {
        if (current_index != entry_of_n_in_p) {
            if (hrtree->type == CONVENTIONAL_HILBERT_RTREE)
                s_nodes[i] = get_hilbertnode(&hrtree->base, parent_n->entries.internal[current_index]->pointer, n_height);
            else if (hrtree->type == FAST_HILBERT_RTREE_TYPE)
                s_nodes[i] = (HilbertRNode*) fb_retrieve_node(&hrtree->base, parent_n->entries.internal[current_index]->pointer, n_height);
            else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE)
                s_nodes[i] = (HilbertRNode*) efind_buf_retrieve_node(&hrtree->base, efind_spc, parent_n->entries.internal[current_index]->pointer, n_height);

#ifdef COLLECT_STATISTICAL_DATA
            if (n_height > 0)
                _visited_int_node_num++;
            else
                _visited_leaf_node_num++;
            insert_reads_per_height(n_height, 1);
#endif
        } else {
            s_nodes[i] = n;
            n = NULL;
        }

        if (s_nodes[i]->type == HILBERT_INTERNAL_NODE) {
            for (j = 0; j < s_nodes[i]->nofentries; j++) {
                entries[nofentries] = s_nodes[i]->entries.internal[j];
                nofentries++;
            }
            lwfree(s_nodes[i]->entries.internal);
        } else {
            for (j = 0; j < s_nodes[i]->nofentries; j++) {
                entries[nofentries] = hilbertentry_create(s_nodes[i]->entries.leaf[j]->pointer,
                        s_nodes[i]->entries.leaf[j]->bbox, 0);
                lwfree(s_nodes[i]->entries.leaf[j]);
                nofentries++;
            }
            lwfree(s_nodes[i]->entries.leaf);
        }

        s_nodes[i]->nofentries = 0; //this is an empty node now

        current_index++;
    }

    //the number of nodes will vary according to the following cases:

    /* U2. if all the node have the minimum number of entries, distribute E
evenly among them according to the Hilbert value.
     */
    if (nofentries >= (s_length * min_entries_per_node)) {
        //we will allow this number of entries per node in the redistribution
        nofentries_per_node = ceil((double) nofentries / (double) s_length);
        if (nofentries_per_node > max_entries_per_node) {
            _DEBUG(ERROR, "Wow, the redistribution is not sufficient!");
        }
        *flag = HILBERT_RED_WITHOUT_MOD;
        //_DEBUGF(NOTICE, "A redistribution is sufficient. Each node will have %d entries", nofentries_per_node);
    } else {
        int p;
        /* U3. if all the siblings are ready to underflow,
merge s + 1 to s nodes, adjust the resulting nodes.  */
        *flag = HILBERT_MERGE;

        nofentries_per_node = ceil((double) nofentries / (double) osp);

        remove_this_entry = start;
        start++;
        if (remove_this_entry != entry_of_n_in_p) {
            hilbertnode_free(s_nodes[0]);
            s_nodes[0] = NULL;
        }
        memmove(s_nodes + 0, s_nodes + (0 + 1), sizeof (s_nodes[0]) * (s_length - 0 - 1));
        s_length--;

        p = parent_n->entries.internal[remove_this_entry]->pointer;

        //we add the removed page as an empty page now
        rtreesinfo_add_empty_page(hrtree->info, p);

        //we then delete this node here
        if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
            del_hilbertnode(&hrtree->base, p, n_height);
        } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
            fb_del_node(&hrtree->base, fast_spc, p, n_height);
        } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
            efind_buf_del_node(&hrtree->base, efind_spc, p, n_height);
        }
    }

#ifdef COLLECT_STATISTICAL_DATA
    _processed_entries_num += nofentries;
#endif

    //_DEBUG(NOTICE, "Distributing the entries");

    /*we make the distribution here*/
    if (type == HILBERT_INTERNAL_NODE) {
        int last = s_length - 1;
        int size_last;
        for (i = 0; i < (s_length - 1); i++) {
            s_nodes[i]->entries.internal = (HilbertIEntry**)
                    lwalloc(nofentries_per_node * sizeof (HilbertIEntry*));
            //we now copy the reference from E
            for (j = 0; j < nofentries_per_node; j++) {
                s_nodes[i]->entries.internal[j] = entries[cur];
                cur++;
            }
            s_nodes[i]->nofentries = nofentries_per_node;
        }

        //we now put the remaining entries in the last node
        size_last = nofentries - cur;
        s_nodes[last]->entries.internal = (HilbertIEntry**)
                lwalloc(size_last * sizeof (HilbertIEntry*));
        //we now copy the reference from E
        for (j = 0; j < size_last; j++) {
            s_nodes[last]->entries.internal[j] = entries[cur];
            cur++;
        }
        s_nodes[last]->nofentries = size_last;
    } else {
        int last = s_length - 1;
        int size_last;
        for (i = 0; i < (s_length - 1); i++) {
            s_nodes[i]->entries.leaf = (REntry**)
                    lwalloc(nofentries_per_node * sizeof (REntry*));
            //we now copy the entry from E
            for (j = 0; j < nofentries_per_node; j++) {
                s_nodes[i]->entries.leaf[j] = rentry_create(entries[cur]->pointer, entries[cur]->bbox);

                lwfree(entries[cur]);
                cur++;
            }

            s_nodes[i]->nofentries = nofentries_per_node;
        }

        //if there are more entries, we put it into the last node
        //we now put the remaining entries in the last node
        size_last = nofentries - cur;
        s_nodes[last]->entries.leaf = (REntry**)
                lwalloc(size_last * sizeof (REntry*));
        //we now copy the reference from E
        for (j = 0; j < size_last; j++) {
            s_nodes[last]->entries.leaf[j] = rentry_create(entries[cur]->pointer, entries[cur]->bbox);

            lwfree(entries[cur]);
            cur++;
        }
        s_nodes[last]->nofentries = size_last;
    }

    //_DEBUG(NOTICE, "Done");

    current_index = start;

    //_DEBUG(NOTICE, "Writing the modifications");
    for (i = 0; i < s_length; i++) {
        if (current_index == entry_of_n_in_p) {
            cur_pointer = n_add;
        } else {
            cur_pointer = parent_n->entries.internal[current_index]->pointer;
        }

        /*we must write the modified nodes!*/
        if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
            put_hilbertnode(&hrtree->base, s_nodes[i], cur_pointer, n_height);
        } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
            //we have to remove the node and then insert it
            fb_del_node(&hrtree->base, fast_spc, cur_pointer, n_height);
            fb_put_new_node(&hrtree->base, fast_spc, cur_pointer,
                    (void*) hilbertnode_clone(s_nodes[i]), n_height);
        } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
            efind_buf_del_node(&hrtree->base, efind_spc, cur_pointer, n_height);
            efind_buf_create_node(&hrtree->base, efind_spc, cur_pointer, n_height);
            if (type == HILBERT_INTERNAL_NODE) {
                for (j = 0; j < s_nodes[i]->nofentries; j++) {
                    efind_buf_mod_node(&hrtree->base, efind_spc, cur_pointer,
                            (void*) hilbertientry_clone(s_nodes[i]->entries.internal[j]), n_height);
                }
            } else {
                for (j = 0; j < s_nodes[i]->nofentries; j++) {
                    efind_buf_mod_node(&hrtree->base, efind_spc, cur_pointer,
                            (void*) rentry_clone(s_nodes[i]->entries.leaf[j]), n_height);
                }
            }
        }

#ifdef COLLECT_STATISTICAL_DATA
        if (n_height > 0)
            _written_int_node_num++;
        else
            _written_leaf_node_num++;
        insert_writes_per_height(n_height, 1);
#endif

        //we adjust the MBR and/or LHV in the parent
        bbox_node = bbox_create();
        h = hilbertnode_compute_bbox(s_nodes[i], spec->srid, bbox_node);
        bbox_entry = parent_n->entries.internal[current_index]->bbox;
        if (!bbox_check_predicate(bbox_node, bbox_entry, EQUAL) || h != parent_n->entries.internal[current_index]->lhv) {
            //we should modify the bbox or lhv
            if (*flag == HILBERT_RED_WITHOUT_MOD)
                *flag = HILBERT_RED_WITH_MOD;

            if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                fb_put_mod_lhv(&hrtree->base, fast_spc, parent_add, h, current_index, n_height + 1);
                fb_put_mod_bbox(&hrtree->base, fast_spc, parent_add, bbox_clone(bbox_node), current_index, n_height + 1);
            }

            memcpy(parent_n->entries.internal[current_index]->bbox, bbox_node, sizeof (BBox));
            parent_n->entries.internal[current_index]->lhv = h;

            //we only apply the modifications for FAST e eFIND since they apply it into the buffer
            //after the return of this function, the caller should write the parent node
            if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                efind_buf_mod_node(&hrtree->base, efind_spc, parent_add,
                        (void*) hilbertientry_clone(parent_n->entries.internal[current_index]), n_height + 1);
            }
        }
        lwfree(bbox_node);

        //we cannot free the n, since it is responsibility for the caller
        if (current_index != entry_of_n_in_p) {
            hilbertnode_free(s_nodes[i]);
        } else {
            n = s_nodes[i];
        }

        current_index++;
    }

    lwfree(s_nodes);
    lwfree(entries);

    return remove_this_entry;
}

HilbertRNode *split1to2(const HilbertRNode *n, HilbertRNode *l) {
    HilbertRNode *nn = hilbertnode_create_empty(n->type);
    int i, j = 0;
    int last_entry;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec startt;
    struct timespec end;

    cpustart = get_CPU_time();
    startt = get_current_time();
#endif   

    last_entry = (n->nofentries / 2);

    nn->nofentries = last_entry;
    l->nofentries = n->nofentries - last_entry;

    if (n->type == HILBERT_INTERNAL_NODE) {
        nn->entries.internal = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * nn->nofentries);
        l->entries.internal = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * l->nofentries);
        for (i = 0; i < last_entry; i++) {
            nn->entries.internal[i] = hilbertientry_clone(n->entries.internal[i]);
        }
        for (; i < n->nofentries; i++) {
            l->entries.internal[j] = hilbertientry_clone(n->entries.internal[i]);
            j++;
        }
    } else {
        nn->entries.leaf = (REntry**) lwalloc(sizeof (REntry*) * nn->nofentries);
        l->entries.leaf = (REntry**) lwalloc(sizeof (REntry*) * l->nofentries);
        for (i = 0; i < last_entry; i++) {
            nn->entries.leaf[i] = rentry_clone(n->entries.leaf[i]);
        }
        for (; i < n->nofentries; i++) {
            l->entries.leaf[j] = rentry_clone(n->entries.leaf[i]);
            j++;
        }
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    //here we consider the split time as the handle_overflow time
    _split_cpu_time += get_elapsed_time(cpustart, cpuend);
    _split_time += get_elapsed_time(startt, end);
#endif  

    return nn;
}

/* Returns the leaf node in which to place
a new rectangle entry with a given hilbert value. */
HilbertRNode *choose_node(HilbertRTree *hrtree, hilbert_value_t hilbert,
        int height, HilbertRNodeStack *stack, int *chosen_address) {
    HilbertRNode *n = NULL;
    hilbert_value_t cur_h;

    int tree_height;

    int i;
    int entry = -1;

    /*C1. Initialize: Set N to be the root node.  */
    n = hilbertnode_clone(hrtree->current_node); //because our stack that stores references
    *chosen_address = hrtree->info->root_page;
    tree_height = hrtree->info->height;

    //_DEBUG(NOTICE, "Starting from the root...");
    //hilbertnode_print(n, *chosen_address);

    /*C2. Leaf check: If N is a leaf, return N.*/
    while (true) {
        entry = -1;
        if (n == NULL) {
            _DEBUG(ERROR, "Node is null in choose_node");
        }
        //yay we found the node N
        if (tree_height == height) {
            return n;
        }

        /*C3. Choose subtree: if N is a non-leaf node, choose the entry
(R, ptr, LHV) with the minimum LHV value greater than h. h here is the hilbert variable*/
        for (i = 0; i < n->nofentries; i++) {
            cur_h = n->entries.internal[i]->lhv;
            //we stop when we found the first element greater than a given hilbert value
            if (hilbert <= cur_h) {
                entry = i;
                break;
            }
        }
        /*if all of them < hilbert, we put it in the last branch*/
        if (entry == -1) {
            entry = n->nofentries - 1;
        }

#ifdef COLLECT_STATISTICAL_DATA
        _processed_entries_num += (entry + 1);
#endif

        //OK we have choose the better path, 
        //then we put it in our stack to adjust this node after
        hilbertnode_stack_push(stack, n, *chosen_address, entry);

        /*C4. Descend until a leaf is reached: set N to the node pointed by ptr and repeat from C2. */
        *chosen_address = n->entries.internal[entry]->pointer;
        if (hrtree->type == CONVENTIONAL_HILBERT_RTREE)
            n = get_hilbertnode(&hrtree->base, *chosen_address, tree_height - 1);
        else if (hrtree->type == FAST_HILBERT_RTREE_TYPE)
            n = (HilbertRNode*) fb_retrieve_node(&hrtree->base, *chosen_address, tree_height - 1);
        else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE)
            n = (HilbertRNode*) efind_buf_retrieve_node(&hrtree->base, efind_spc, *chosen_address, tree_height - 1);
        else
            _DEBUGF(ERROR, "Invalid Hilbert R-tree specification %d", hrtree->type);

       // hilbertnode_print(n, *chosen_address);

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

HilbertRNode *adjust_tree(HilbertRTree *hrtree, HilbertRNode *l, HilbertRNode *ll,
        int *split_address, int *removed_entry, int l_height, HilbertRNodeStack *stack, uint8_t flag) {
    BBox *n_bbox, *bbox_entry;
    hilbert_value_t n_h, h_entry;
    int parent_add;
    int entry;

    int h = l_height; //this is the height of the node to be inserted
    HilbertRNode *n = NULL, *nn = NULL;
    bool adjusting = true;
    uint8_t typemod = flag;

    /*AT1 [Initialize.] Set N=L If L was split previously, set NN to be the resulting
second node */
    if (l != NULL) //l is null only after a merging operation
        n = hilbertnode_clone(l);
    if (ll != NULL)
        nn = hilbertnode_clone(ll);

    hilbertnode_free(hrtree->current_node);
    hrtree->current_node = NULL;

    /*AT2 [Check If done ] If N is the root, stop*/
    while (h != hrtree->info->height) {
        /*adjust the MBR’s and LHV’s in the parent level*/
        /*if we are not adjusting the tree anymore because it is not more necessary
         we stop!*/
        if (!adjusting)
            break;

        //current_node will be the parent of n        
        hrtree->current_node = hilbertnode_stack_pop(stack, &parent_add, &entry);

        /*now we process the following situations:
         * DIRECT: means that we only must check the parent node, without other modifications
         * RED_WITH_MOD: means that the overflow/underflow handling distributed the entries of n and changed the bbox and lhv where was needed
         * RED_WITHOUT_MOD: means that the overflow/underflow handling distributed the entries of n and did not change the bbox and/or lhv of its parent
         * SPLIT: means that the overflow handling distributed the entries of n, changed the bbox and lhv, and created a new node, nn
         * MERGE: means that the underflow handling distributed the entries of n and excluded an entry of its parent
         * 
         * the nodes where occurred the redistribution were previously written by the handle_overflow/underflow!
         * the only exception were: 
         * for the handle_overflow:
         *      in case of a split, the new node and the parent node! 
         * for the handle_underflow:
         *      in case of a merge, the parent node!
         */

        if (typemod == HILBERT_DIRECT) {
            n_bbox = bbox_create();
            n_h = hilbertnode_compute_bbox(n, hrtree->spec->srid, n_bbox);
            //we adjust the MBR and/or LHV in the parent for its modified child            
            bbox_entry = hrtree->current_node->entries.internal[entry]->bbox;
            h_entry = hrtree->current_node->entries.internal[entry]->lhv;
            if (!bbox_check_predicate(n_bbox, bbox_entry, EQUAL) || n_h != h_entry) {

                //we before check the modification for FAST (because of the memcpy below)
                if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                    fb_put_mod_lhv(&hrtree->base, fast_spc, parent_add, n_h, entry, h + 1);
                    fb_put_mod_bbox(&hrtree->base, fast_spc, parent_add, bbox_clone(n_bbox), entry, h + 1);
                }

                //we should modify the bbox or/and lhv
                memcpy(hrtree->current_node->entries.internal[entry]->bbox, n_bbox, sizeof (BBox));
                hrtree->current_node->entries.internal[entry]->lhv = n_h;

                if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                    put_hilbertnode(&hrtree->base, hrtree->current_node, parent_add, h + 1);
                } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                    efind_buf_mod_node(&hrtree->base, efind_spc, parent_add,
                            (void*) hilbertientry_clone(hrtree->current_node->entries.internal[entry]), h + 1);
                }

                adjusting = true;

#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num++;
                insert_writes_per_height(h + 1, 1);
#endif
                //_DEBUG(NOTICE, "Parent node after the adjustment: ");
               // hilbertnode_print(hrtree->current_node, parent_add);

                //we move up to next level (see T5 below)
                hilbertnode_free(n);
                n = hrtree->current_node;
                hrtree->current_node = NULL;
            } else {
                adjusting = false;
            }
            lwfree(n_bbox);
        } else if (typemod == HILBERT_RED_WITHOUT_MOD) {
            adjusting = false;
        } else if (typemod == HILBERT_RED_WITH_MOD) {
            
           // _DEBUG(NOTICE, "Parent node after the redistribution: ");
            //hilbertnode_print(hrtree->current_node, parent_add);
            
            //in this case, the handle_overflow already modified the parent
            if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                //thus, we only have to write the parent node
                put_hilbertnode(&hrtree->base, hrtree->current_node, parent_add, h + 1);
                //for the eFIND and FAST indices the modifications were already applied in the handle overflow!
            }
            //we move up to next level (see T5 below)
            hilbertnode_free(n);
            n = hrtree->current_node;
            hrtree->current_node = NULL;
            //we check now for the next levels in the same way of a direct insert
            typemod = HILBERT_DIRECT;

#ifdef COLLECT_STATISTICAL_DATA
            _written_int_node_num++;
            insert_writes_per_height(h + 1, 1);
#endif
        } else if (typemod == HILBERT_SPLIT) {
            //in this case, the handle_overflow already modified the parent entries and we should add the new entry because of split
            BBox *bbox_split = bbox_create(); //we cannot free it because it is added in hrtree->current_node!!!
            hilbert_value_t h_split;
            int pos;

            h_split = hilbertnode_compute_bbox(nn, hrtree->spec->srid, bbox_split);

           // _DEBUG(NOTICE, "Creating a new entry in the parent to accommodate the split node");

            pos = hilbertnode_add_entry(hrtree->current_node,
                    (void*) hilbertentry_create(*split_address, bbox_split, h_split), h_split, hrtree->spec->srid);

            //now we should check if this node is with overcapacity
            if (hrtree->current_node->nofentries <= hrtree->spec->max_entries_int_node) {
                //then we write this node
                if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                    //thus, we only have to write the parent node
                    put_hilbertnode(&hrtree->base, hrtree->current_node, parent_add, h + 1);
                } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                    //we put the new pointer, new hilbert value and new bbox for the split node
                    if (pos != hrtree->current_node->nofentries - 1)
                        fb_put_mod_hole(&hrtree->base, fast_spc, parent_add, pos, h + 1);
                    fb_put_mod_pointer(&hrtree->base, fast_spc, parent_add, *split_address, pos, h + 1);
                    fb_put_mod_lhv(&hrtree->base, fast_spc, parent_add, h_split, pos, h + 1);
                    fb_put_mod_bbox(&hrtree->base, fast_spc, parent_add, bbox_clone(bbox_split), pos, h + 1);
                    fb_completed_insertion();
                } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                    //we put the new entry which points to the split node
                    efind_buf_mod_node(&hrtree->base, efind_spc, parent_add,
                            (void *) hilbertentry_create(*split_address, bbox_clone(bbox_split), h_split), h + 1);
                } else {
                    _DEBUGF(ERROR, "Invalid Hilbert R-tree specification %d", hrtree->type);
                }

#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num++;
                insert_writes_per_height(h + 1, 1);
#endif
                //_DEBUG(NOTICE, "parent node after inserting the split node:");
                //hilbertnode_print(hrtree->current_node, parent_add);

                //we did not perform split
                hilbertnode_free(n);
                hilbertnode_free(nn);
                n = hrtree->current_node;
                nn = NULL;
                hrtree->current_node = NULL;
                typemod = HILBERT_DIRECT;
            } else {
                //we must handle the node with overflow
                HilbertRNode *parent;
                int n_add;

                hilbertnode_free(n);
                hilbertnode_free(nn);

                n = hrtree->current_node;
                hrtree->current_node = NULL;
                n_add = parent_add;

                parent = hilbertnode_stack_peek(stack, &parent_add, &entry);
                if (parent == NULL) {
                    HilbertRNode *aux = hilbertnode_create_empty(HILBERT_INTERNAL_NODE);
                    typemod = HILBERT_SPLIT;
                    //this indicates that n is the root node, then we apply the 1-to-2 split
                    nn = split1to2(n, aux);
                    hilbertnode_free(n);
                    n = aux;

                    //_DEBUG(NOTICE, "Split 1 to 2 because this is the root node");

                    //we need to apply the modifications of n
                    if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                        //we write the n               
                        put_hilbertnode(&hrtree->base, n, n_add, h + 1);
                    } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                        //remove the old version
                        fb_del_node(&hrtree->base, fast_spc, n_add, h + 1);
                        //we put the node in the buffer
                        fb_put_new_node(&hrtree->base, fast_spc, n_add,
                                (void *) hilbertnode_clone(n), h + 1);
                    } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                        int in;
                        //remove the old version
                        efind_buf_del_node(&hrtree->base, efind_spc, n_add, h + 1);
                        //we put the newly created node
                        efind_buf_create_node(&hrtree->base, efind_spc, n_add, h + 1);
                        for (in = 0; in < n->nofentries; in++) {
                            efind_buf_mod_node(&hrtree->base, efind_spc, n_add,
                                    (void *) hilbertientry_clone(n->entries.internal[in]), h + 1);
                        }
                    }

                    //_DEBUG(NOTICE, "New version of the split node: ");
                    //hilbertnode_print(n, n_add);

#ifdef COLLECT_STATISTICAL_DATA
                    _written_int_node_num += 1;
                    insert_writes_per_height(h + 1, 1);
#endif
                } else {

                    //_DEBUG(NOTICE, "Handling overflow inside the adjust tree");

                    nn = handle_overflow(hrtree, n, n_add, h + 1, entry, parent, parent_add, &typemod);
                    //at this point we have the parent node split and its entries redistributed, 
                    //the parent of parent entries were also modified accordingly
                }

                //we do not free n here since it is used in other situations and freed accordingly

                if (nn != NULL && typemod == HILBERT_SPLIT) {
                    //we have to write the nn
                    *split_address = rtreesinfo_get_valid_page(hrtree->info);
                    if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                        //we write the created node by the split with a new number page                
                        put_hilbertnode(&hrtree->base, nn, *split_address, h + 1);
                    } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                        //we put the new node in the buffer
                        fb_put_new_node(&hrtree->base, fast_spc, *split_address,
                                (void *) hilbertnode_clone(nn), h + 1);
                    } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                        int in;
                        //we put the newly created node
                        efind_buf_create_node(&hrtree->base, efind_spc, *split_address, h + 1);
                        for (in = 0; in < nn->nofentries; in++) {
                            efind_buf_mod_node(&hrtree->base, efind_spc, *split_address,
                                    (void *) hilbertientry_clone(nn->entries.internal[in]), h + 1);
                        }
                    }

                    //_DEBUG(NOTICE, "New node (to be inserted into the parent in the next iteration): ");
                    //hilbertnode_print(nn, *split_address);

#ifdef COLLECT_STATISTICAL_DATA
                    _written_int_node_num += 1;
                    insert_writes_per_height(h + 1, 1);
#endif
                }

            }
        } else if (typemod == HILBERT_MERGE) {
            int r_p = hrtree->current_node->entries.internal[*removed_entry]->pointer;

            //in this case, the handle_underflow already modified the parent entries and we should remove the entry because of merging
            hilbertnode_remove_entry(hrtree->current_node, *removed_entry);

            //now we should check if this node is under underflow
            if (hrtree->current_node->nofentries >= hrtree->spec->min_entries_int_node) {
                //then we write this node
                if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                    //thus, we only have to write the parent node
                    put_hilbertnode(&hrtree->base, hrtree->current_node, parent_add, h + 1);
                } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                    //we remove the entry
                    fb_put_mod_bbox(&hrtree->base, fast_spc, parent_add, NULL, *removed_entry, h + 1);
                } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                    //we remove the entry
                    efind_buf_mod_node(&hrtree->base, efind_spc, parent_add,
                            (void *) hilbertentry_create(r_p, NULL, 0), h + 1);
                } else {
                    _DEBUGF(ERROR, "Invalid Hilbert R-tree specification %d", hrtree->type);
                }
                *removed_entry = -1;

#ifdef COLLECT_STATISTICAL_DATA
                _written_int_node_num++;
                insert_writes_per_height(h + 1, 1);
#endif

                //we did not perform merge
                hilbertnode_free(n);
                n = hrtree->current_node;
                hrtree->current_node = NULL;
                typemod = HILBERT_DIRECT;
            } else {
                //we must handle the node with underflow
                HilbertRNode *parent;
                int n_add;

                hilbertnode_free(n);

                n = hrtree->current_node;
                hrtree->current_node = NULL;
                n_add = parent_add;

                parent = hilbertnode_stack_peek(stack, &parent_add, &entry);
                if (parent == NULL) {
                    //this means that n is the root node and we can just skip it
                    //we will later check if the tree should be cut
                } else {
                    *removed_entry = handle_underflow(hrtree, n, n_add, h + 1, entry, parent, parent_add, &typemod);
                    //at this point we have the parent node merged and its entries redistributed, 
                    //the parent of parent entries were also modified accordingly
                }

                //we do not free n here since it is used in other situations that free it accordingly
            }
        }

        h++;
    }

    /*we stopped to adjusted the tree, we set the current_node as the root node here*/
    if (!adjusting) {
        while (stack->size > 0) {
            hilbertnode_free(hrtree->current_node);
            hrtree->current_node = hilbertnode_stack_pop(stack, &parent_add, &entry);
        }
        hilbertnode_free(n);
    } else {
        hrtree->current_node = n;
    }
    if (nn != NULL && nn->nofentries == 0) {
        hilbertnode_free(nn);
    }
    return nn;
}

bool insert_entry(HilbertRTree *hrtree, REntry *input) {
    HilbertRNode *chosen_node; //the node in which was chosen to insert the input
    int chosen_address = 0; //page number of the chosen_node

    //insertions are always in a leaf node
    int max_entries = hrtree->spec->max_entries_leaf_node;

    HilbertRNode *ll = NULL; //the split node (that is, the LL)
    int split_address = -1; //page number of the split node
    HilbertRNodeStack *stack = hilbertnode_stack_init(); //to adjust_tree
    HilbertRNode *new = NULL; //in case of the split occurred until the root node
    uint8_t typemod;
    int pos;

    /*hv is the Hilbert value of the rectangle*/
    hilbert_value_t hv = hilbertvalue_compute(input->bbox, hrtree->spec->srid);

    //_DEBUG(NOTICE, "inserting a new entry. CHoosing the right leaf node.");

    /*I1. Find the appropriate leaf node:
        Invoke ChooseLeaf(r, h) to select a leaf node L in which to place P.
     L is our chosen_node here*/
    chosen_node = choose_node(hrtree, hv, 0, stack, &chosen_address);

    //_DEBUG(NOTICE, "leaf node has been chosen");

    //we add the entry in order here without checking the size
    pos = hilbertnode_add_entry(chosen_node, (void*) input, hv, hrtree->spec->srid);

    //_DEBUG(NOTICE, "the new entry was added:");
    //hilbertnode_print(chosen_node, chosen_address);

    /*I2. Insert r in a leaf node L:
            if L has an empty slot, insert r in L in the appropriate place according to the Hilbert
order and return.  */
    if (chosen_node->nofentries <= max_entries) {
        /*this is an direct insert*/
        typemod = HILBERT_DIRECT;
        /*then we can write the node with the new entry*/
        if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
            //thus, we only have to write the parent node
            put_hilbertnode(&hrtree->base, chosen_node, chosen_address, 0);
        } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
            //we put the new pointer, and new bbox
            if (pos != chosen_node->nofentries - 1)
                fb_put_mod_hole(&hrtree->base, fast_spc, chosen_address, pos, 0);
            fb_put_mod_pointer(&hrtree->base, fast_spc, chosen_address, input->pointer, pos, 0);
            fb_put_mod_bbox(&hrtree->base, fast_spc, chosen_address, bbox_clone(input->bbox), pos, 0);
            fb_completed_insertion();
        } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
            //we put the new entry
            efind_buf_mod_node(&hrtree->base, efind_spc, chosen_address,
                    (void *) rentry_clone(input), 0);
        } else {
            _DEBUGF(ERROR, "Invalid Hilbert R-tree specification %d", hrtree->type);
        }

       // _DEBUG(NOTICE, "direct insert has been processed");

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif       

    } else { /*I2 (continuation). if L is full, invoke HandleOverflow(L,r), which will return new leaf if split was inevitable. */

        /*if the tree has height equal to 0, then we make the split1to2*/
        if (hrtree->info->height == 0) {
            HilbertRNode *aux = hilbertnode_create_empty(HILBERT_LEAF_NODE);

            //_DEBUG(NOTICE, "Splitting the root node split 1 to 2");

            typemod = HILBERT_SPLIT;
            //this indicates that n is the root node, then we apply the 1-to-2 split
            ll = split1to2(chosen_node, aux);
            hilbertnode_free(chosen_node);
            chosen_node = aux;

            //we have to assign a new valid page number for the split node
            split_address = rtreesinfo_get_valid_page(hrtree->info);

            //_DEBUG(NOTICE, "New version of the split node: ");
            //hilbertnode_print(chosen_node, chosen_address);
            //_DEBUG(NOTICE, "New node: ");
            //hilbertnode_print(ll, split_address);

            //we need to apply the modifications of chosen_node
            if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                //we write the chosen_node               
                put_hilbertnode(&hrtree->base, chosen_node, chosen_address, 0);
                //we write the created node by the split with a new number page                
                put_hilbertnode(&hrtree->base, ll, split_address, 0);
            } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                //remove the old version of chosen_node
                fb_del_node(&hrtree->base, fast_spc, chosen_address, 0);
                //we put the node in the buffer
                fb_put_new_node(&hrtree->base, fast_spc, chosen_address,
                        (void *) hilbertnode_clone(chosen_node), 0);

                //we put the new node in the buffer
                fb_put_new_node(&hrtree->base, fast_spc, split_address,
                        (void *) hilbertnode_clone(ll), 0);
            } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                int in;
                //remove the old version
                efind_buf_del_node(&hrtree->base, efind_spc, chosen_address, 0);
                //we put the new version
                efind_buf_create_node(&hrtree->base, efind_spc, chosen_address, 0);
                for (in = 0; in < chosen_node->nofentries; in++) {
                    efind_buf_mod_node(&hrtree->base, efind_spc, chosen_address,
                            (void *) rentry_clone(chosen_node->entries.leaf[in]), 0);
                }

                //we put the newly created node
                efind_buf_create_node(&hrtree->base, efind_spc, split_address, 0);
                for (in = 0; in < ll->nofentries; in++) {
                    efind_buf_mod_node(&hrtree->base, efind_spc, split_address,
                            (void *) rentry_clone(ll->entries.leaf[in]), 0);
                }
            }

            //_DEBUG(NOTICE, "split done");

#ifdef COLLECT_STATISTICAL_DATA
            _written_leaf_node_num += 2;
            insert_writes_per_height(0, 2);
#endif
        } else
            /*otherwise, we call the handle_overflow*/ {
            HilbertRNode *parent;
            int entry_of_n_in_p, parent_add;

            //_DEBUG(NOTICE, "Processing the handle overflow");

            parent = hilbertnode_stack_peek(stack, &parent_add, &entry_of_n_in_p);
            
            //_DEBUG(NOTICE, "Parent node is:");
            //hilbertnode_print(parent, parent_add);
            
            ll = handle_overflow(hrtree, chosen_node, chosen_address, 0, entry_of_n_in_p, parent, parent_add, &typemod);
            
            //_DEBUG(NOTICE, "Parent node (after the handle overflow) is:");
            //hilbertnode_print(parent, parent_add);
            
            if (ll != NULL && typemod == HILBERT_SPLIT) {
                //we have to write the ll
                split_address = rtreesinfo_get_valid_page(hrtree->info);
                if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                    //we write the created node by the split with a new number page                
                    put_hilbertnode(&hrtree->base, ll, split_address, 0);
                } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                    //we put the new node in the buffer
                    fb_put_new_node(&hrtree->base, fast_spc, split_address,
                            (void *) hilbertnode_clone(ll), 0);
                } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                    int in;
                    //we put the newly created node
                    efind_buf_create_node(&hrtree->base, efind_spc, split_address, 0);
                    for (in = 0; in < ll->nofentries; in++) {
                        efind_buf_mod_node(&hrtree->base, efind_spc, split_address,
                                (void *) rentry_clone(ll->entries.leaf[in]), 0);
                    }
                }

                //_DEBUG(NOTICE, "New node created in the handle overflow: ");
                //hilbertnode_print(ll, split_address);

#ifdef COLLECT_STATISTICAL_DATA
                _written_leaf_node_num += 1;
                insert_writes_per_height(0, 1);
#endif
            }

            //_DEBUG(NOTICE, "Handle overflow done");

        }
    }

    //_DEBUG(NOTICE, "Adjusting the tree");

    /*I3. Propagate changes upward:
            form a set S that contains L, its cooperating siblings and the new leaf (if any). 
            invoke AdjustTree 
     We employ a stack here to do it
     the adjust_tree also sets the hrtree->current_node to the root node of the tree
     the split_address now stores the address of the new node if the root node was also split
     note that some part of the required adjustments were made on the handle_overflow*/
    new = adjust_tree(hrtree, chosen_node, ll, &split_address, NULL, 0, stack, typemod);

    //_DEBUG(NOTICE, "Tree adjusted");

    /*14. Grow tree taller:
            if node split propagation caused the root to
            split, create a new root whose children are
            the two resulting nodes.*/
    if (new != NULL) {
        //then we have to create a new root with 2 entries
        HilbertRNode *new_root = hilbertnode_create_empty(HILBERT_INTERNAL_NODE);
        int new_root_add;

        //first entry is the current_node, which is the old root,
        //while the second entry is the split node
        HilbertIEntry *entry1, *entry2;
        BBox *bbox_entry1, *bbox_entry2;
        hilbert_value_t hv_entry1, hv_entry2;

        //_DEBUG(NOTICE, "Growing up the tree");

        //we allocate one more page for the new root
        new_root_add = rtreesinfo_get_valid_page(hrtree->info);

        //the height of the tree is incremented
        hrtree->info->height++;

        bbox_entry1 = bbox_create();
        bbox_entry2 = bbox_create();
        hv_entry1 = hilbertnode_compute_bbox(hrtree->current_node, hrtree->spec->srid, bbox_entry1);
        hv_entry2 = hilbertnode_compute_bbox(new, hrtree->spec->srid, bbox_entry2);

        entry1 = hilbertentry_create(hrtree->info->root_page, bbox_entry1, hv_entry1);
        entry2 = hilbertentry_create(split_address, bbox_entry2, hv_entry2);

        //we add the new two entries into the new root node
        hilbertnode_add_entry(new_root, (void*) entry1, hv_entry1, hrtree->spec->srid);
        hilbertnode_add_entry(new_root, (void*) entry2, hv_entry2, hrtree->spec->srid);

        //we write the new root node
        if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
            put_hilbertnode(&hrtree->base, new_root, new_root_add, hrtree->info->height);
        } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
            fb_put_new_node(&hrtree->base, fast_spc, new_root_add,
                    (void *) hilbertnode_clone(new_root), hrtree->info->height);
        } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
            //we firstly set the new height of the tree
            if (efind_spc->read_buffer_policy == eFIND_HLRU_RBP)
                efind_readbuffer_hlru_set_tree_height(hrtree->info->height);

            efind_buf_create_node(&hrtree->base, efind_spc, new_root_add, hrtree->info->height);
            efind_buf_mod_node(&hrtree->base, efind_spc, new_root_add,
                    (void *) hilbertientry_clone(entry1), hrtree->info->height);
            efind_buf_mod_node(&hrtree->base, efind_spc, new_root_add,
                    (void *) hilbertientry_clone(entry2), hrtree->info->height);
        } else {
            _DEBUGF(ERROR, "Invalid Hilbert R-tree specification %d", hrtree->type);
        }
        storage_update_tree_height(&hrtree->base, hrtree->info->height);

        //_DEBUG(NOTICE, "New root node created:");
        //hilbertnode_print(new_root, new_root_add);

#ifdef COLLECT_STATISTICAL_DATA
        _written_int_node_num++;
        insert_writes_per_height(hrtree->info->height, 1);
#endif

        //we update the root page
        hrtree->info->root_page = new_root_add;

        hilbertnode_free(hrtree->current_node);
        hrtree->current_node = new_root;
        hilbertnode_free(new);
    }
    hilbertnode_free(chosen_node);
    if (ll != NULL)
        hilbertnode_free(ll);
    hilbertnode_stack_destroy(stack);
   // _DEBUG(NOTICE, "New entry was successfully inserted");

    return true;
}

bool remove_entry(HilbertRTree *hrtree, REntry *rem) {
    HilbertRNodeStack *stack;
    HilbertRNode *found_node = NULL;
    int found_add = -1;
    int found_index = -1;
    int i;
    hilbert_value_t hv;

    hv = hilbertvalue_compute(rem->bbox, hrtree->spec->srid);

    stack = hilbertnode_stack_init();

    //_DEBUG(NOTICE, "Searching subtrees");
    /*D1. Find the host leaf:
            Perform an exact match search to find
            the leaf node L that contain r. */
    found_node = choose_node(hrtree, hv, 0, stack, &found_add);

    //the chosen_node indicates the node that probably have the entry to be removed
    //let's check it
    for (i = 0; i < found_node->nofentries; i++) {
        //it is always a leaf node
        if (found_node->entries.leaf[i]->pointer == rem->pointer) {
            //yay, we found it!
            found_index = i;
            break;
        }
    }
    //but, if we did not find the entry, let us to search on the siblings
    //because the siblings may have repeated hilbert values
    if (found_index == -1 && hrtree->info->height > 0) {
        HilbertRNode *parent = NULL;
        HilbertRNode *aux = NULL;
        int j;
        int pointer;

        parent = hilbertnode_stack_peek(stack, NULL, NULL);
        for (i = 0; i < parent->nofentries; i++) {
            pointer = parent->entries.internal[i]->pointer;
            //we did not consider the previous checked node
            if (pointer != found_add) {
                if (hrtree->type == CONVENTIONAL_HILBERT_RTREE)
                    aux = get_hilbertnode(&hrtree->base, pointer, 0);
                else if (hrtree->type == FAST_HILBERT_RTREE_TYPE)
                    aux = (HilbertRNode*) fb_retrieve_node(&hrtree->base, pointer, 0);
                else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE)
                    aux = (HilbertRNode*) efind_buf_retrieve_node(&hrtree->base, efind_spc, pointer, 0);
                else
                    _DEBUGF(ERROR, "Invalid Hilbert R-tree specification %d", hrtree->type);

#ifdef COLLECT_STATISTICAL_DATA
                _visited_leaf_node_num++;
                insert_reads_per_height(0, 1);
#endif

                for (j = 0; j < aux->nofentries; j++) {
                    //it is always a leaf node
                    if (aux->entries.leaf[j]->pointer == rem->pointer) {
                        //yay, we found it!
                        found_index = i;
                        break;
                    }
                }
                if (found_index != -1) {
                    hilbertnode_free(found_node);
                    found_node = aux;
                    break;
                } else {
                    hilbertnode_free(aux);
                }
            }
        }
    }

    //_DEBUG(NOTICE, "Done");

    /*D2. Delete r. Remove r from L*/
    if (found_index != -1 && found_node != NULL) {
        int entry_to_be_removed = -1;
        uint8_t typemod = 0;
        hilbertnode_remove_entry(found_node, found_index);

        /*D3. if L underflows
           borrow some entries from s cooperating siblings.
            if all the siblings are ready to underflow,
            merge s + 1 to s nodes, adjust the resulting nodes.
         */
        if (found_node->nofentries < hrtree->spec->min_entries_leaf_node
                && hrtree->info->height > 0) {
            int parent_add, entry_of_n_in_p;
            HilbertRNode *parent;

            //_DEBUG(NOTICE, "Calling the handle underflow");
            parent = hilbertnode_stack_peek(stack, &parent_add, &entry_of_n_in_p);
            //yes, we need to employ the handle_underflow
            entry_to_be_removed = handle_underflow(hrtree, found_node, found_add, 0, entry_of_n_in_p, parent, parent_add, &typemod);
            //_DEBUG(NOTICE, "Done");
        } else {
            //we can write this node here without problems
            typemod = HILBERT_DIRECT;

            if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                put_hilbertnode(&hrtree->base, found_node, found_add, 0);
            } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                //we remove the entry
                fb_put_mod_bbox(&hrtree->base, fast_spc, found_add, NULL, found_index, 0);
            } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                //we remove the entry
                efind_buf_mod_node(&hrtree->base, efind_spc, found_add,
                        (void *) rentry_create(rem->pointer, NULL), 0);
            }
        }

        //_DEBUG(NOTICE, "Propagating the changes");
        /* D4. adjust MBR and LHV in parent level:
                form a set S that contains L and its cooperating siblings (if underflow has occurred). */
        adjust_tree(hrtree, found_node, NULL, NULL, &entry_to_be_removed, 0, stack, typemod);
        hilbertnode_free(found_node);
        //_DEBUG(NOTICE, "Tree adjusted");

        //handling the root node here
        if (entry_to_be_removed != -1) {
            int r_p;
            if (hrtree->info->height > 0)
                r_p = hrtree->current_node->entries.internal[entry_to_be_removed]->pointer;
            else
                r_p = hrtree->current_node->entries.leaf[entry_to_be_removed]->pointer;
            hilbertnode_remove_entry(hrtree->current_node, entry_to_be_removed);

            if (hrtree->current_node->nofentries == 1 && hrtree->info->height > 0) {
                /*[Shorten tree.] If the root node has only one child after the tree has
    been adjusted, make the child the new root*/
                int p = hrtree->current_node->entries.leaf[0]->pointer;
                HilbertRNode *new_root = NULL;

                if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                    del_hilbertnode(&hrtree->base, hrtree->info->root_page, hrtree->info->height);
                } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                    fb_del_node(&hrtree->base, fast_spc, hrtree->info->root_page, hrtree->info->height);
                } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                    //we update the height of the tree
                    if (efind_spc->read_buffer_policy == eFIND_HLRU_RBP)
                        efind_readbuffer_hlru_set_tree_height(hrtree->info->height - 1);
                    efind_buf_del_node(&hrtree->base, efind_spc,
                            hrtree->info->root_page, hrtree->info->height);
                }
                storage_update_tree_height(&hrtree->base, hrtree->info->height - 1);
                hilbertnode_free(hrtree->current_node);

                //we add the removed page as an empty page now
                rtreesinfo_add_empty_page(hrtree->info, hrtree->info->root_page);

#ifdef COLLECT_STATISTICAL_DATA
                _deleted_int_node_num++;
                insert_writes_per_height(hrtree->info->height, 1);
#endif

                hrtree->info->root_page = p;

                if (hrtree->type == CONVENTIONAL_HILBERT_RTREE)
                    new_root = get_hilbertnode(&hrtree->base, p, hrtree->info->height - 1);
                else if (hrtree->type == FAST_HILBERT_RTREE_TYPE)
                    new_root = (HilbertRNode *) fb_retrieve_node(&hrtree->base, p, hrtree->info->height - 1);
                else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                    new_root = (HilbertRNode *) efind_buf_retrieve_node(&hrtree->base, efind_spc,
                            p, hrtree->info->height - 1);
                }

#ifdef COLLECT_STATISTICAL_DATA
                if (hrtree->info->height > 1) {
                    //we visited one internal node, then we add it
                    _visited_int_node_num++;
                } else {
                    //we visited one leaf node
                    _visited_leaf_node_num++;
                }
                insert_reads_per_height(hrtree->info->height - 1, 1);
#endif

                hrtree->current_node = new_root;
                hrtree->info->height--;
            } else {
                //we can write this node here without problems
                if (hrtree->type == CONVENTIONAL_HILBERT_RTREE) {
                    //thus, we only have to write the parent node
                    put_hilbertnode(&hrtree->base, hrtree->current_node, hrtree->info->root_page, hrtree->info->height);
                } else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                    //we remove the entry
                    fb_put_mod_bbox(&hrtree->base, fast_spc, hrtree->info->root_page, NULL, entry_to_be_removed, hrtree->info->height);
                } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                    //we remove the entry
                    if (hrtree->info->height > 0)
                        efind_buf_mod_node(&hrtree->base, efind_spc, hrtree->info->root_page,
                            (void *) hilbertentry_create(r_p, NULL, 0), hrtree->info->height);
                    else
                        efind_buf_mod_node(&hrtree->base, efind_spc, hrtree->info->root_page,
                            (void *) rentry_create(r_p, NULL), hrtree->info->height);
                }
            }

        }
    }

    hilbertnode_stack_destroy(stack);

    //_DEBUG(NOTICE, "Entry removed");
    if (found_index != -1)
        return true;
    else
        return false;
}

/*********************************
 * functions in order to make HilbertRTree a standard SpatialIndex (see spatial_index.h)
 *********************************/
static uint8_t hilbertrtree_get_type(const SpatialIndex *si) {
    HilbertRTree *hrtree = (void *) si;

    return hrtree->type;
}

static bool hilbertrtree_insert(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    BBox *bbox = (BBox*) lwalloc(sizeof (BBox));
    HilbertRTree *hrtree = (void *) si;
    REntry *input;

    /*we should check the information regarding the SRID because of the hilbert values*/
    if (hrtree->spec->srid != geom->srid && hrtree->spec->srid != 0) {
        _DEBUGF(ERROR, "SRID does not match on the Hilbert index (%d) with the inserting geometry (%d)",
                hrtree->spec->srid, geom->srid);
    }
    hrtree->spec->srid = geom->srid;

    gbox_to_bbox(geom->bbox, bbox);

    input = rentry_create(pointer, bbox);

    //we only insert new entries on the leaf, therefore it is an REntry

    return insert_entry(hrtree, input);
}

static bool hilbertrtree_remove(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    BBox *bbox = (BBox*) lwalloc(sizeof (BBox));
    HilbertRTree *hrtree = (void *) si;
    REntry *rem;

    gbox_to_bbox(geom->bbox, bbox);
    rem = rentry_create(pointer, bbox);

    //we only remove entries on the leaf, therefore it is an REntry

    return remove_entry(hrtree, rem);
}

static bool hilbertrtree_update(SpatialIndex *si, int oldpointer, const LWGEOM *oldgeom, int newpointer, const LWGEOM *newgeom) {
    bool r, i = false;
    //TO-DO improve the return value of this.. i.e., it can return an error identifier.
    r = hilbertrtree_remove(si, oldpointer, oldgeom);
    if (r)
        i = hilbertrtree_insert(si, newpointer, newgeom);

    return r && i;
}

static SpatialIndexResult *hilbertrtree_search_ss(SpatialIndex *si, const LWGEOM *search_object, uint8_t predicate) {
    SpatialIndexResult *sir;
    BBox *search = (BBox*) lwalloc(sizeof (BBox));
    HilbertRTree *hrtree = (void *) si;

    gbox_to_bbox(search_object->bbox, search);

    sir = hilbertrtree_search(hrtree, search, predicate);

    lwfree(search);

    return sir;
}

static bool hilbertrtree_header_writer(SpatialIndex *si, const char *file) {
    festival_header_writer(file, CONVENTIONAL_HILBERT_RTREE, si);

    return true;
}

static void hilbertrtree_destroy(SpatialIndex *si) {

    HilbertRTree *hrtree = (void *) si;
    hilbertnode_free(hrtree->current_node);
    lwfree(hrtree->spec);
    rtreesinfo_free(hrtree->info);

    generic_parameters_free(hrtree->base.gp);
    lwfree(hrtree->base.index_file);
    source_free(hrtree->base.src);

    lwfree(hrtree);
}

/* return a new (empty) Hilbert R-tree index, it only specifies the general parameters but not the specific parameters! */
SpatialIndex *hilbertrtree_empty_create(char *file, Source *src, GenericParameters *gp,
        BufferSpecification *bs, bool persist) {
    HilbertRTree *hrtree;

    /*define the general functions of the hilbertrtree*/
    static const SpatialIndexInterface vtable = {hilbertrtree_get_type,
        hilbertrtree_insert, hilbertrtree_remove, hilbertrtree_update, hilbertrtree_search_ss,
        hilbertrtree_header_writer, hilbertrtree_destroy};
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    hrtree = (HilbertRTree*) lwalloc(sizeof (HilbertRTree));
    memcpy(&hrtree->base, &base, sizeof (base));
    hrtree->type = CONVENTIONAL_HILBERT_RTREE; //this is a conventional r-tree

    hrtree->spec = (HilbertRTreeSpecification*) lwalloc(sizeof (HilbertRTreeSpecification));
    hrtree->info = rtreesinfo_create(0, 0, 0);

    hrtree->current_node = NULL;

    //we have to persist the empty node
    if (persist) {
        //the root node is a leaf node
        hrtree->current_node = hilbertnode_create_empty(HILBERT_LEAF_NODE);
        put_hilbertnode(&hrtree->base, hrtree->current_node, hrtree->info->root_page, hrtree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &hrtree->base;
}
