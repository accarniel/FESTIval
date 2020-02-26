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

#include "split.h"

#include <float.h> /*mininum and maximum values of double values */

#include "../main/log_messages.h" /*for messages */
#include "../main/math_util.h" /*for the functions with double values */

#include "../main/statistical_processing.h" /* to collect statistical data */

static void exponential_split_node(const RTreeSpecification *rs, RNode *input, int input_level, RNode *l, RNode *ll);

/*these are for the quadratic_split_node. They remove selected entries from the input!*/
static void quadratic_pick_seeds(RNode *input, int *entry1, int *entry2);
static void quadratic_pick_next(RNode *input, const BBox *bbox_l, const BBox *bbox_ll, int *next);
/*these are for the linear_split_node. They remove selected entries from the input! */
static void linear_pick_seeds(RNode *input, int *entry1, int *entry2);
static void linear_pick_next(RNode *input, int *next);

/* Split algorithm of Greene:
 * Reference: GREENE, D. An implementation and performance analysis of spatial data access methods. 
 * IEEE  International Conference on Data Engineering, p. 606-615, 1989.
 * 
 * Partially implemented by Kairo Bonicenha
 */
static void greene_split(RNode *input, int input_level, RNode *l, RNode *ll);

/* Split algorithm of AngTang:
 * Reference: ANG, C-H. & TAN, T.C. New Linear Node Splitting Algorithm for R-trees. 
 * In Proceedings of the 5th International Symposium on Advances in Spatial Databases (SSD '97), p. 339-349, 1997. * 
 */
static void angtan_split(RNode *input, RNode *l, RNode *ll);

/*for exponential split node -> we need a function to compute all combinations of splits*/
static int next_comb(int *comb, int k, int n);
static int difference(const int *original, int original_n, int *result, int total);

/*This function is the next_combination
        comb => the previous combination ( use (0, 1, 2, ..., k) for first)
        k => the size of the subsets to generate
        n => the size of the original set

        Returns: 1 if a valid combination was found
                0, otherwise
 */
int next_comb(int *comb, int k, int n) {
    int i = k - 1;
    ++comb[i];
    while ((i > 0) && (comb[i] >= n - k + 1 + i)) {
        --i;
        ++comb[i];
    }

    if (comb[0] > n - k) /* Combination (n-k, n-k+1, ..., n) reached */
        return 0; /* No more combinations can be generated */

    /* comb now looks like (..., x, n, n, n, ..., n).
    Turn it into (..., x, x + 1, x + 2, ...) */
    for (i = i + 1; i < k; ++i)
        comb[i] = comb[i - 1] + 1;

    return 1;
}

/*it returns the number of element in the result*/
int difference(const int *original, int original_n, int *result, int total) {
    int i;
    int j;
    int k;
    bool put;
    j = 0;
    for (i = 0; i < total; i++) {
        put = true;
        for (k = 0; k < original_n; k++) {
            if (i == original[k]) {
                put = false;
                break;
            }
        }
        if (put) {
            result[j] = i;
            j++;
        }
    }
    return j;
}

void exponential_split_node(const RTreeSpecification *rs, RNode *input, int input_level, RNode *l, RNode *ll) {
    int i;
    int n_ll; //number of elements in ll
    int n = input->nofentries;
    int k;
    int *combination_l = (int*) lwalloc(sizeof (int)*n);
    int *combination_ll = (int*) lwalloc(sizeof (int)*n);
    RNode *temp_l, *temp_ll;
    BBox *bbox_l, *bbox_ll;
    double best_area_l, best_area_ll;
    double temp_area_l, temp_area_ll;
    int min_entries;

    if (input_level == 0) {
        min_entries = rs->min_entries_leaf_node;
    } else {
        min_entries = rs->min_entries_int_node;
    }

    k = min_entries; //we generate combination with size equal to k

    /*setup the first combination for l*/
    for (i = 0; i < k; ++i) {
        combination_l[i] = i;
    }

    /*setup the first combination for ll*/
    n_ll = difference(combination_l, k, combination_ll, n);
    if (n_ll < k) {
        _DEBUG(ERROR, "The first iteration of exponential split node generated am invalid ll node");
    }

    /* we split the node according to their combination
     * note that we add copies of the input entries! */
    /*for l*/
    for (i = 0; i < k; ++i) {
        rnode_add_rentry(l, rentry_clone(input->entries[combination_l[i]]));
    }
    /*for ll*/
    for (i = 0; i < n_ll; ++i) {
        rnode_add_rentry(ll, rentry_clone(input->entries[combination_ll[i]]));
    }

    /*we calculate the first bboxes*/
    bbox_l = rnode_compute_bbox(l);
    bbox_ll = rnode_compute_bbox(ll);
    /*then their areas*/
    best_area_l = bbox_area(bbox_l);
    best_area_ll = bbox_area(bbox_ll);
    lwfree(bbox_l);
    lwfree(bbox_ll);

    temp_l = rnode_create_empty();
    temp_ll = rnode_create_empty();

    //check if l and ll will have the minimum entries, otherwise stop
    while (n - k >= min_entries) {

        //while we have other valid combinations for l
        while (next_comb(combination_l, k, n)) {
            //we calculate the combination for ll
            n_ll = difference(combination_l, k, combination_ll, n);
            if (n_ll < min_entries) {
                _DEBUGF(ERROR,
                        "An iteration of exponential split node generated an invalid ll node with %d entries", n_ll);
            }

            /* we process the splits to the temporary nodes
             * note that we add copies of the input entries! */
            /*for temp_l*/
            for (i = 0; i < k; ++i) {
                rnode_add_rentry(temp_l, rentry_clone(input->entries[combination_l[i]]));
            }
            /*for temp_ll*/
            for (i = 0; i < n_ll; ++i) {
                rnode_add_rentry(temp_ll, rentry_clone(input->entries[combination_ll[i]]));
            }

            /*we calculate the their bboxes*/
            bbox_l = rnode_compute_bbox(temp_l);
            bbox_ll = rnode_compute_bbox(temp_ll);
            /*we check if this combination is better than the previous one*/
            temp_area_l = bbox_area(bbox_l);
            temp_area_ll = bbox_area(bbox_ll);
            lwfree(bbox_l);
            lwfree(bbox_ll);
            if (best_area_l > temp_area_l && best_area_ll > temp_area_ll) {
                best_area_l = temp_area_l;
                best_area_ll = temp_area_ll;
                //we update the l and ll
                rnode_copy(l, temp_l);
                rnode_copy(ll, temp_ll);
            }
            //then go to the next possible combination...
            rnode_free(temp_l);
            rnode_free(temp_ll);
            temp_l = rnode_create_empty();
            temp_ll = rnode_create_empty();
        }
        //when all combinations for k is done, we try to do combinations with k++

        k++;
    }
    rnode_free(temp_l);
    rnode_free(temp_ll);
    lwfree(combination_l);
    lwfree(combination_ll);
}

void quadratic_pick_seeds(RNode *input, int *entry1, int *entry2) {
    int i, j;
    double max_waste, waste;
    int ent1 = 0, ent2 = 0;
    double total_area, area1, area2;

    max_waste = -1.0 * DBL_MAX;

    /*PS1 [Calculate inefficiency of grouping entries together] For each pair of
entries E1 and E2, compose a rectangle J including E1I and E2I 
     * Calculate d = area(J) - area(E1I) - area(E2I)*/
    for (i = 0; i < input->nofentries; i++) {
        area1 = bbox_area(input->entries[i]->bbox);
        for (j = i + 1; j < input->nofentries; j++) {
            area2 = bbox_area(input->entries[j]->bbox);
            total_area = bbox_area_of_union(input->entries[i]->bbox, input->entries[j]->bbox);
            waste = total_area - area1 - area2;
            /*PS2 [Choose the most wasteful pair ] Choose the pair with the largest d*/
            if (waste > max_waste) {
                ent1 = i;
                ent2 = j;
                max_waste = waste;
            }
        }
    }
    //set the entries according to the found ent1 and ent2
    *entry1 = ent1;
    *entry2 = ent2;
}

void quadratic_pick_next(RNode *input, const BBox *bbox_l, const BBox *bbox_ll, int *next) {
    int i;
    double expanded_area1, expanded_area2;
    double max_diff, diff;
    int entry = 0;

    max_diff = -1.0 * DBL_MAX;

    /*PN1 [Determine cost of putting each entry in each group ] For each entry
E not yet in a group, calculate d1 = the area increase required in the
covering rectangle of Group 1 to include EI
Calculate d2 similarly for Group 2*/
    for (i = 0; i < input->nofentries; i++) {
        expanded_area1 = bbox_area_of_required_expansion(input->entries[i]->bbox, bbox_l);
        expanded_area2 = bbox_area_of_required_expansion(input->entries[i]->bbox, bbox_ll);
        diff = fabs(expanded_area2 - expanded_area1);
        /*PN2 [Find entry with greatest preference for one group ] Choose any entry
         * with the maximum difference between d1 and d2*/
        if (diff >= max_diff) {
            max_diff = diff;
            entry = i;
        }
    }
    *next = entry;
}

void linear_pick_seeds(RNode *input, int *entry1, int *entry2) {
    double highest_low_side;
    double lowest_high_side;
    double length_max, length_min;
    int highest_low_index, lowest_high_index;
    bool found;
    double separation, best_separation;

    int ent1 = 0, ent2 = 0;

    int i, j;

    found = false;
    best_separation = 0.0;

    /*LPS1 [Find extreme rectangles along all dimensions ] Along each dimension,
find the entry whose rectangle has the highest low side, and the one with the lowest high side 
     * Record the separation*/
    for (i = 0; i <= MAX_DIM; i++) {
        //for each dimension (e.g., x and y) we compute a best separation
        highest_low_index = -1;
        lowest_high_index = -1;

        highest_low_side = -1.0 * DBL_MAX;
        length_max = -1.0 * DBL_MAX;

        length_min = DBL_MAX;
        lowest_high_side = DBL_MAX;

        for (j = 0; j < input->nofentries; j++) {
            //these help us to compute the width of the low (min) and high (max) side
            length_min = DB_MIN(input->entries[j]->bbox->min[i], length_min);
            length_max = DB_MAX(input->entries[j]->bbox->max[i], length_max);

            //these get the highest low (min) side and the lowest high (max) side
            if (input->entries[j]->bbox->min[i] > highest_low_side) {
                highest_low_side = input->entries[j]->bbox->min[i];
                highest_low_index = j;
            }
            if (input->entries[j]->bbox->max[i] < lowest_high_side) {
                lowest_high_side = input->entries[j]->bbox->max[i];
                lowest_high_index = j;
            }
        }
        /*LPS2 [AdJust for shape of the rectangle cluster ] Normalize the separations
by dividing by the width of the entire set along the corresponding dimension*/
        separation = (lowest_high_index == highest_low_index) ? -1.0 :
                fabs((lowest_high_side - highest_low_side) / (length_max - length_min));

        /*LPS3 [Select the most extreme pair ] Choose the pair with the greatest
normalized separation along any dimension*/
        if (separation > best_separation) {
            ent1 = lowest_high_index;
            ent2 = highest_low_index;
            best_separation = separation;
            found = true;
        }
    }

    /*there is a possibility that we have no found the entries
     it can happens if all rectangles in the input overlap*/
    if (!found) {
        /*for the dimension 2, we can solve it by considering the entry with lowest Y
         and the entry with the largest X (they must be always different rectangles)*/
        if (NUM_OF_DIM == 2) {
            double miny, maxx;
            ent1 = -1;
            ent2 = -1;

            miny = input->entries[0]->bbox->min[1];
            maxx = input->entries[0]->bbox->max[0];
            for (j = 1; j < input->nofentries; j++) {
                //this get the highest low (min) side and the lowest high (max) side
                if (input->entries[j]->bbox->min[1] < miny) {
                    miny = input->entries[j]->bbox->min[1];
                    ent2 = j;
                } else if (input->entries[j]->bbox->max[0] > maxx) {
                    maxx = input->entries[j]->bbox->max[0];
                    ent1 = j;
                }
            }
        } else {
            /*otherwise, we get the first 2 entries*/
            ent1 = 0;
            ent2 = 1;
        }
    }
    *entry1 = ent1;
    *entry2 = ent2;
}

/*PickNext for linear simply chooses any of the remaining entries */
void linear_pick_next(RNode *input, int *next) {
    if (input->nofentries >= 1)
        *next = 0;
    else
        _DEBUG(ERROR, "Input has no elements at linear_pick_next");
}

/*the caller is responsible to free the input RNODE!!!
 l and ll are empty nodes and they are the result of the split*/
void split_node(const RTreeSpecification *rs, RNode *input, int input_height, RNode *l, RNode *ll) {
    uint8_t type = rs->split_type;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif   

    /*we can also call the r*-tree split for r-tree...*/
    if (type == RSTARTREE_SPLIT) {
        RStarTreeSpecification sp;
        sp.max_entries_int_node = rs->max_entries_int_node;
        sp.max_entries_leaf_node = rs->max_entries_leaf_node;
        sp.min_entries_int_node = rs->min_entries_int_node;
        sp.min_entries_leaf_node = rs->min_entries_leaf_node;
        rstartree_split_node(&sp, input, input_height, l, ll);
        return;
    } else if (type == GREENE_SPLIT) {
        greene_split(input, input_height, l, ll);
        return;
    } else if (type == ANGTAN_SPLIT) {
        angtan_split(input, l, ll);
        return;
    }

#ifdef COLLECT_STATISTICAL_DATA
    if (input_height != 0)
        _split_int_num++;
    else
        _split_leaf_num++;
#endif

    if (type == RTREE_EXPONENTIAL_SPLIT) {
        exponential_split_node(rs, input, input_height, l, ll);
    } else {
        REntry *entry1 = NULL;
        REntry *entry2 = NULL;
        int ent1 = 0, ent2 = 0;
        REntry *next = NULL;
        int next_entry;
        int i;
        BBox *bbox_l, *bbox_ll;
        BBox *temp_l, *temp_ll;
        double area_l, area_ll;
        int min_entries;

        //check if the split is consistent with r-tree
        if (type != RTREE_LINEAR_SPLIT && type != RTREE_QUADRATIC_SPLIT) {
            _DEBUGF(ERROR, "There is no split type for %d in R-tree", type);
        }

        if (input_height == 0) {
            min_entries = rs->min_entries_leaf_node;
        } else {
            min_entries = rs->min_entries_int_node;
        }

        /*QS1 [Pick first entry for each group ] Apply Algorithm PickSeeds to choose
    two entries to be the first elements of the groups 
         * Assign each to a group*/
        //_DEBUG(NOTICE, "picking seeds");
        if (type == RTREE_LINEAR_SPLIT)
            linear_pick_seeds(input, &ent1, &ent2);
        else
            quadratic_pick_seeds(input, &ent1, &ent2);

        //set the entries according to the found lowest and highest indices
        entry1 = rentry_clone(input->entries[ent1]);
        entry2 = rentry_clone(input->entries[ent2]);
        //remove them from the input
        if (ent1 > ent2) {
            rnode_remove_rentry(input, ent1);
            rnode_remove_rentry(input, ent2);
        } else {
            rnode_remove_rentry(input, ent2);
            rnode_remove_rentry(input, ent1);
        }

        //we add these first entries to their respective nodes l and ll
        rnode_add_rentry(l, entry1);
        rnode_add_rentry(ll, entry2);

        //we set the bbox of each node (l and ll)
        bbox_l = bbox_create();
        memcpy(bbox_l, l->entries[0]->bbox, sizeof (BBox));

        bbox_ll = bbox_create();
        memcpy(bbox_ll, ll->entries[0]->bbox, sizeof (BBox));

        temp_l = bbox_create();
        temp_ll = bbox_create();

        /* QS2 [Check If done ] If all entries have been assigned, stop 
         * If one group has so few entries that all the rest must be assigned 
         * to it in order for it to have the minimum number m, assign them and stop*/
        while (input->nofentries > 0) {
            if ((l->nofentries >= min_entries) &&
                    (ll->nofentries + input->nofentries == min_entries)) {
                //therefore we have to add all the remaining items from input to ll
                for (i = 0; i < input->nofentries; i++) {
                    //if this entry exist (i.e., it was not previously assigned)
                    if (input->entries[i] != NULL) {
                        rnode_add_rentry(ll, rentry_clone(input->entries[i]));
                    }
                }
                break;
            }
            if ((ll->nofentries >= min_entries) &&
                    (l->nofentries + input->nofentries == min_entries)) {
                //therefore we have to add all the remaining items from input to l
                for (i = 0; i < input->nofentries; i++) {
                    //if this entry exist (i.e., it was not previously assigned)
                    if (input->entries[i] != NULL) {
                        rnode_add_rentry(l, rentry_clone(input->entries[i]));
                    }
                }
                break;
            }
            /*QS3 [Select entry to assign ] Invoke Algorithm PickNext to choose the next
    entry to assign 
             * Add it to the group whose covering rectangle will have to
    be enlarged least to accommodate it 
             * Resolve ties by adding the entry to the group with smaller area, then to
    the one with fewer entries, then to either 
             * Repeat from QS2*/
            //_DEBUG(NOTICE, "picking next entry");
            if (type == RTREE_LINEAR_SPLIT) {
                linear_pick_next(input, &next_entry);
            } else {
                quadratic_pick_next(input, bbox_l, bbox_ll, &next_entry);
            }

            next = rentry_clone(input->entries[next_entry]);
            rnode_remove_rentry(input, next_entry);

            bbox_expanded_area_and_union(next->bbox, bbox_l, temp_l, &area_l);
            bbox_expanded_area_and_union(next->bbox, bbox_ll, temp_ll, &area_ll);

            //when we decide where to put the next entry, we update its respective bbox
            if (area_l < area_ll) {
                //then we put this entry into l
                rnode_add_rentry(l, next);
                //therefore, we need to update the bbox_l
                memcpy(bbox_l, temp_l, sizeof (BBox));
            } else if (area_ll < area_l) {
                //then we put this entry into ll
                rnode_add_rentry(ll, next);
                //therefore, we need to update the bbox_ll
                memcpy(bbox_ll, temp_ll, sizeof (BBox));
            } else {
                //we have to solve this tie
                area_l = bbox_area(bbox_l);
                area_ll = bbox_area(bbox_ll);
                if (area_l < area_ll) {
                    //then we put this entry into l
                    rnode_add_rentry(l, next);
                    //therefore, we need to update the bbox_l
                    memcpy(bbox_l, temp_l, sizeof (BBox));
                } else if (area_ll < area_l) {
                    //then we put this entry into ll
                    rnode_add_rentry(ll, next);
                    //therefore, we need to update the bbox_ll
                    memcpy(bbox_ll, temp_ll, sizeof (BBox));
                } else {
                    //tie again
                    if (l->nofentries < ll->nofentries) {
                        //then we put this entry into l                    
                        rnode_add_rentry(l, next);
                        //therefore, we need to update the bbox_l
                        memcpy(bbox_l, temp_l, sizeof (BBox));
                    } else if (ll->nofentries < l->nofentries) {
                        //then we put this entry into l
                        rnode_add_rentry(ll, next);
                        //therefore, we need to update the bbox_ll
                        memcpy(bbox_ll, temp_ll, sizeof (BBox));
                    } else {
                        //the last tie, then we put into the l
                        rnode_add_rentry(l, next);
                        //therefore, we need to update the bbox_l
                        memcpy(bbox_l, temp_l, sizeof (BBox));
                    }
                }
            }
        }
        lwfree(bbox_l);
        lwfree(bbox_ll);
        lwfree(temp_l);
        lwfree(temp_ll);
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _split_cpu_time += get_elapsed_time(cpustart, cpuend);
    _split_time += get_elapsed_time(start, end);
#endif    
}

/*global variable to store the current dimension to be considered in the comparator of the qsort*/
static int _dimension = 0;
static int lower_comp_entry(const void *a, const void *b);
static int upper_comp_entry(const void *a, const void *b);
static double compute_sum_margin_values(const REntry **entries, int nofentries, int min_entries, int k);
static int choose_split_index(const REntry **lower_dist, const REntry **upper_dist, int nofentries, int min_entries, int k, uint8_t *chosen_dist);

/*comparators for the rstartree split node*/
int lower_comp_entry(const void* a, const void* b) {
    // a is a pointer into the array of pointers
    REntry *ptr_to_left_struct = *(REntry**) a;
    REntry *ptr_to_right_struct = *(REntry**) b;

    if (DB_LT(ptr_to_left_struct->bbox->min[_dimension], ptr_to_right_struct->bbox->min[_dimension]))
        return -1;
    else if (DB_GT(ptr_to_left_struct->bbox->min[_dimension], ptr_to_right_struct->bbox->min[_dimension]))
        return 1;
    else
        return 0;
}

int upper_comp_entry(const void* a, const void* b) {
    // a is a pointer into the array of pointers
    REntry *ptr_to_left_struct = *(REntry**) a;
    REntry *ptr_to_right_struct = *(REntry**) b;

    if (DB_LT(ptr_to_left_struct->bbox->max[_dimension], ptr_to_right_struct->bbox->max[_dimension]))
        return -1;
    else if (DB_GT(ptr_to_left_struct->bbox->max[_dimension], ptr_to_right_struct->bbox->max[_dimension]))
        return 1;
    else
        return 0;
}

double compute_sum_margin_values(const REntry **entries, int nofentries, int min_entries, int k) {
    double margin = 0.0;
    int nentries;
    int i;

    /*(ii) margin-value = margin[bb(first group)] + margin[bb(second group)]
     bb = bounding box*/

    //for each possible distribution
    for (i = 1; i <= k; i++) {
        /*The first group contains the first (m-1)+k entries,*/
        nentries = min_entries - 1 + i;
        margin += rentry_margin(entries, nentries);
        /* the second group contains the remaining entries*/
        margin += rentry_margin(&entries[nentries], nofentries - nentries);
    }
    return margin;
}

int choose_split_index(const REntry **lower_dist, const REntry **upper_dist, int nofentries, int min_entries, int k, uint8_t *chosen_dist) {
    double leastarea, area, overlap, leastoverlap;
    int i, n;
    int chosen_k = 0;
    BBox bbox1;
    BBox bbox2;

    leastarea = DBL_MAX;
    leastoverlap = DBL_MAX;

    /*CSI1 Along the chosen split axIs, choose the distribution with the minimum
     *  overlap-value Resolve ties by choosing the distribution with
        minimum area-value*/
    for (i = 1; i <= k; i++) {
        /*The first group contains the first (m-1)+k entries,*/
        n = min_entries - 1 + i;
        /* the second group contains the remaining entries*/

        /*we firstly process the lower_distribution*/

        rentry_create_bbox(lower_dist, n, &bbox1);
        rentry_create_bbox(&lower_dist[n], nofentries - n, &bbox2);

        if (bbox_check_predicate(&bbox1, &bbox2, INTERSECTS)) {
            overlap = bbox_overlap_area(&bbox1, &bbox2);
        } else {
            overlap = 0.0;
        }

        if (overlap < leastoverlap) {
            leastoverlap = overlap;
            chosen_k = i;
            *chosen_dist = 0;
        } else if (DB_IS_EQUAL(overlap, leastoverlap)) {
            area = bbox_area(&bbox1) + bbox_area(&bbox2);
            if (area < leastarea) {
                leastarea = area;
                chosen_k = i;
                *chosen_dist = 0;
            }
        }

        /*then we process the upper_distribution*/

        rentry_create_bbox(upper_dist, n, &bbox1);
        /* the second group contains the remaining entries*/
        rentry_create_bbox(&upper_dist[n], nofentries - n, &bbox2);

        if (bbox_check_predicate(&bbox1, &bbox2, INTERSECTS)) {
            overlap = bbox_overlap_area(&bbox1, &bbox2);
        } else {
            overlap = 0.0;
        }

        if (overlap < leastoverlap) {
            leastoverlap = overlap;
            chosen_k = i;
            *chosen_dist = 1;
        } else if (DB_IS_EQUAL(overlap, leastoverlap)) {
            area = bbox_area(&bbox1) + bbox_area(&bbox2);
            if (area < leastarea) {
                leastarea = area;
                chosen_k = i;
                *chosen_dist = 1;
            }
        }
    }
    return chosen_k;
}

void rstartree_split_node(const RStarTreeSpecification *rs, RNode *input, int input_height, RNode *l, RNode *ll) {
    REntry **lower_distributions;
    REntry **upper_distributions;
    int max_entries, min_entries;
    int k, i, chosen_axis = 0, chosen_k = 0;
    uint8_t chosen_dist;
    /* k the number of distributions
     * i is the index of a for
     * chosen_axis indicates which axis is better to get (x or y for two-dimensional data)
     * chosen_dist refers to if we choose the upper or lower distribution
     * chosen_k is the distribution that we choose
     */
    double leastmargin, lower_summargin, upper_summargin;
    int j, n;
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();

    if (input_height != 0)
        _split_int_num++;
    else
        _split_leaf_num++;
#endif

    if (input_height == 0) {
        min_entries = rs->min_entries_leaf_node;
        max_entries = rs->max_entries_leaf_node;
    } else {
        min_entries = rs->min_entries_int_node;
        max_entries = rs->max_entries_int_node;
    }
    //the number of possible distributions such as defined in the original paper
    k = max_entries - (2 * min_entries) + 2;
    //we will have two versions of possible distributions
    //sort by lower values
    lower_distributions = (REntry**) lwalloc(sizeof (REntry*) * (max_entries + 1) * NUM_OF_DIM);
    //sort by upper values
    upper_distributions = (REntry**) lwalloc(sizeof (REntry*) * (max_entries + 1) * NUM_OF_DIM);

    leastmargin = DBL_MAX;

    /*S1 Invoke ChooseSplitAxis to determine the axis, perpendicular to which the split is performed*/

    /*ChooseSplitAxis is inlined here!*/

    /*CSA1 For each axis*/
    for (i = 0; i < NUM_OF_DIM; i++) {
        //we need to copy the input entries for each type of distribution
        for (j = 0; j < input->nofentries; j++) {
            lower_distributions[(i * input->nofentries) + j] = rentry_clone(input->entries[j]);
            upper_distributions[(i * input->nofentries) + j] = rentry_clone(input->entries[j]);
        }
        _dimension = i;

        /*Sort the entries by the lower then by the upper value of their rectangles 
         * and determine all distributions as described above. Compute S the
sum of all margin-values of the different distributions*/

        //sort considering the lower value (min value)
        qsort(&lower_distributions[i * input->nofentries], input->nofentries, sizeof (REntry*), lower_comp_entry);
        lower_summargin = compute_sum_margin_values((const REntry**) &lower_distributions[i * input->nofentries], input->nofentries, min_entries, k);

        //sort considering the upper value (max value)
        qsort(&upper_distributions[i * input->nofentries], input->nofentries, sizeof (REntry*), upper_comp_entry);
        upper_summargin = compute_sum_margin_values((const REntry**) &upper_distributions[i * input->nofentries], input->nofentries, min_entries, k);

        /*CSA2 Choose the axis with the minimum S as split axis*/
        if ((lower_summargin + upper_summargin) < leastmargin) {
            leastmargin = lower_summargin + upper_summargin;
            chosen_axis = i;
        }
    }

    /*S2 Invoke ChooseSplitIndex to determine the best distribution into two groups along that axis*/
    j = chosen_axis * input->nofentries;
    chosen_dist = 0;

    chosen_k = choose_split_index((const REntry**) &lower_distributions[j], (const REntry**) &upper_distributions[j], input->nofentries, min_entries, k, &chosen_dist);

    /*S3 Distribute the entries into two groups*/
    /*The first group contains the first (m-1)+k entries,*/
    n = min_entries - 1 + chosen_k;
    for (i = 0; i < n; i++) {
        //we choose the lower_distribution
        if (chosen_dist == 0) {
            rnode_add_rentry(l, rentry_clone(lower_distributions[j + i]));
        } else {
            rnode_add_rentry(l, rentry_clone(upper_distributions[j + i]));
        }
    }

    /* the second group contains the remaining entries*/
    for (i = n; i < input->nofentries; i++) {
        //we choose the lower_distribution
        if (chosen_dist == 0) {
            rnode_add_rentry(ll, rentry_clone(lower_distributions[j + i]));
        } else {
            rnode_add_rentry(ll, rentry_clone(upper_distributions[j + i]));
        }
    }

    /*freeing memory*/
    for (i = 0; i < NUM_OF_DIM * input->nofentries; i++) {
        if (lower_distributions[i]->bbox)
            lwfree(lower_distributions[i]->bbox);
        lwfree(lower_distributions[i]);
        if (upper_distributions[i]->bbox)
            lwfree(upper_distributions[i]->bbox);
        lwfree(upper_distributions[i]);
    }
    lwfree(upper_distributions);
    lwfree(lower_distributions);
#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _split_cpu_time += get_elapsed_time(cpustart, cpuend);
    _split_time += get_elapsed_time(start, end);
#endif    
}

void greene_split(RNode *input, int input_level, RNode *l, RNode *ll) {
    int i, j;
    //seeds
    REntry *entry1;
    REntry *entry2;
    int ent1, ent2;

    int choose_axis;
    int first_entries;
    //variables to choose the axis
    double length_max, length_min;
    double highest_low_side, lowest_high_side;
    double separation, best_separation;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();

    if (input_level != 0)
        _split_int_num++;
    else
        _split_leaf_num++;
#endif

    /* CA1 Invoke PickSeeds (quadratic pick seeds) to find the two most
distant rectangles of the current node */
    quadratic_pick_seeds(input, &ent1, &ent2);

    //set the entries according to the found lowest and highest indices
    entry1 = rentry_clone(input->entries[ent1]);
    entry2 = rentry_clone(input->entries[ent2]);

    /*CA2 For each axis record the separation of the two seed*/
    best_separation = -1;
    choose_axis = 0;
    for (i = 0; i <= MAX_DIM; i++) {
        highest_low_side = entry1->bbox->min[i];
        lowest_high_side = entry1->bbox->max[i];

        //these get the highest low (min) side and the lowest high (max) side
        if (entry2->bbox->min[i] > highest_low_side) {
            highest_low_side = entry2->bbox->min[i];
        }
        if (entry2->bbox->max[i] < lowest_high_side) {
            lowest_high_side = entry2->bbox->max[i];
        }

        length_max = -1.0 * DBL_MAX;
        length_min = DBL_MAX;
        for (j = 0; j < input->nofentries; j++) {
            //these help us to compute the width of the low (min) and high (max) side
            length_min = DB_MIN(input->entries[j]->bbox->min[i], length_min);
            length_max = DB_MAX(input->entries[j]->bbox->max[i], length_max);
        }

        /*CA3 Normalize the separations by dividing them by the
length of the nodes enclosing rectangle along the
appropriate axis*/
        separation = fabs((lowest_high_side - highest_low_side) / (length_max - length_min));

        /*CA4 Return the axis with the greatest normalized
separation*/
        if (separation > best_separation) {
            choose_axis = i;
        }
    }

    _dimension = choose_axis;
    /*Dl Sort the entries by the low value of then rectangles
along the chosen axis*/
    qsort(&(input->entries), input->nofentries, sizeof (REntry*), lower_comp_entry);

    first_entries = (int) (input->nofentries) / 2;

    /*D2 Assign the first (M+l) div 2 entries to one group, the
last (M+l) dlv 2 entries to the other*/
    for (i = 0; i < first_entries; i++) {
        rnode_add_rentry(l, rentry_clone(input->entries[i]));
    }

    if (input->nofentries % 2 == 0) {
        for (i = (first_entries + 1); i < input->nofentries; i++) {
            rnode_add_rentry(ll, rentry_clone(input->entries[i]));
        }
    } else {
        /*D3 If M+1 is odd, then assign the remaining entry to the
group whose enclosing rectangle will be
increased least by its addition*/

        REntry *remaining_entry;
        double aux, aux2;
        BBox *bbox_l;
        BBox *bbox_ll;
        for (i = (first_entries + 2); i < input->nofentries; i++) {
            rnode_add_rentry(ll, rentry_clone(input->entries[i]));
        }
        remaining_entry = rentry_clone(input->entries[first_entries + 1]);

        bbox_l = rnode_compute_bbox(l);
        bbox_ll = rnode_compute_bbox(ll);

        aux = bbox_area_of_required_expansion(remaining_entry->bbox, bbox_l);
        aux2 = bbox_area_of_required_expansion(remaining_entry->bbox, bbox_ll);

        if (aux < aux2) {
            rnode_add_rentry(l, remaining_entry);
        } else if (aux2 < aux) {
            rnode_add_rentry(ll, remaining_entry);
        } else {
            double area_l = bbox_area(bbox_l);
            double area_ll = bbox_area(bbox_ll);

            if (area_l < area_ll) {
                rnode_add_rentry(l, remaining_entry);
            } else if (area_ll < area_l) {
                rnode_add_rentry(ll, remaining_entry);
            } else {
                rnode_add_rentry(l, remaining_entry);
            }
        }
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _split_cpu_time += get_elapsed_time(cpustart, cpuend);
    _split_time += get_elapsed_time(start, end);
#endif

}

static void angtan_distribution(REntry **list1, REntry **list2,
        int list1_size, int list2_size,
        RNode *l, RNode *ll) {
    int i;

    for (i = 0; i < list1_size; i++) {
        rnode_add_rentry(l, rentry_clone(list1[i]));
    }
    for (i = 0; i < list2_size; i++) {
        rnode_add_rentry(ll, rentry_clone(list2[i]));
    }
}

/* compute the total overlapping area between two list of entries */
static double angtan_total_overlap(REntry **list1, REntry **list2,
        int list1_size, int list2_size) {
    int i;
    int j;
    double ovp_area = 0.0;

    for (i = 0; i < list1_size; i++) {
        for (j = 0; j < list2_size; j++) {
            if (bbox_check_predicate(list1[i]->bbox, list2[j]->bbox, INTERSECTS))
                ovp_area += bbox_overlap_area(list1[i]->bbox, list2[j]->bbox);
        }
    }
    return ovp_area;
}

/* compute the total coverage area of two list of entries */
static double angtan_total_coverage(const REntry **list1, const REntry **list2,
        int list1_size, int list2_size) {
    double cov_area = 0.0;
    BBox *un_list1, *un_list2;

    un_list1 = bbox_create();
    un_list2 = bbox_create();

    rentry_create_bbox(list1, list1_size, un_list1);
    rentry_create_bbox(list2, list2_size, un_list2);

    cov_area = bbox_area(un_list1) + bbox_area(un_list2);

    lwfree(un_list1);
    lwfree(un_list2);

    return cov_area;
}

void angtan_split(RNode *input, RNode *l, RNode *ll) {
    REntry **list_left, **list_right, **list_bottom, **list_top;
    int size_list_l, size_list_r, size_list_b, size_list_t;
    int i;
    BBox *bbox_entry, *bbox_node;

    /*we only implemented the split version fot eh two-dimensional case*/
    if (NUM_OF_DIM > 2) {
        _DEBUGF(ERROR, "The current version of the angtan split"
                "only considers the two-dimensional space."
                " You are considering %d-dimensional space", NUM_OF_DIM);
        return;
    }

    /*first step: make four empty lists
     LISTL <- LISTR <- LISTB <- LISTT = EMPTY     
     */
    list_left = (REntry**) lwalloc(sizeof (REntry*) * input->nofentries);
    list_right = (REntry**) lwalloc(sizeof (REntry*) * input->nofentries);
    list_bottom = (REntry**) lwalloc(sizeof (REntry*) * input->nofentries);
    list_top = (REntry**) lwalloc(sizeof (REntry*) * input->nofentries);
    size_list_l = 0;
    size_list_r = 0;
    size_list_b = 0;
    size_list_t = 0;

    bbox_node = rnode_compute_bbox(input);

    /*For each rectangle S = (xl, yl, xh, yh) in the overflowed node N with 
     * RN = (L, B, R, T)*/
    for (i = 0; i < input->nofentries; i++) {
        bbox_entry = input->entries[i]->bbox;
        if (DB_LT(bbox_entry->min[0] - bbox_node->min[0],
                bbox_node->max[0] - bbox_entry->max[0])) {
            list_left[size_list_l] = input->entries[i];
            size_list_l++;
        } else {
            list_right[size_list_r] = input->entries[i];
            size_list_r++;
        }

        if (DB_LT(bbox_entry->min[1] - bbox_node->min[1],
                bbox_node->max[1] - bbox_entry->max[1])) {
            list_bottom[size_list_b] = input->entries[i];
            size_list_b++;
        } else {
            list_top[size_list_t] = input->entries[i];
            size_list_t++;
        }
    }

    if (DB_MAX(size_list_l, size_list_r) < DB_MAX(size_list_b, size_list_t)) {
        //then split the node along the x direction
        angtan_distribution(list_left, list_right, size_list_l, size_list_r, l, ll);
    } else if (DB_MAX(size_list_l, size_list_r) > DB_MAX(size_list_b, size_list_t)) {
        //then split the node along the y direction
        angtan_distribution(list_bottom, list_top, size_list_b, size_list_t, l, ll);
    } else {
        //tie breaker
        double overlap_x, overlap_y;
        overlap_x = angtan_total_overlap(list_left, list_right, size_list_l, size_list_r);
        overlap_y = angtan_total_overlap(list_bottom, list_top, size_list_b, size_list_t);
        if (overlap_x < overlap_y) {
            //then split the node along the x direction
            angtan_distribution(list_left, list_right, size_list_l, size_list_r, l, ll);
        } else if (overlap_x > overlap_y) {
            //then split the node along the y direction
            angtan_distribution(list_bottom, list_top, size_list_b, size_list_t, l, ll);
        } else {
            //split the node along the direction with smallest total coverage
            double coverage_x, coverage_y;
            coverage_x = angtan_total_coverage((const REntry**) list_left, (const REntry**) list_right, size_list_l, size_list_r);
            coverage_y = angtan_total_coverage((const REntry**) list_bottom, (const REntry**) list_top, size_list_b, size_list_t);
            if (coverage_x < coverage_y) {
                angtan_distribution(list_left, list_right, size_list_l, size_list_r, l, ll);
            } else if (coverage_x > coverage_y) {
                angtan_distribution(list_bottom, list_top, size_list_b, size_list_t, l, ll);
            } else {
                //tie again, this case is not treated by the original algorithm
                //we therefore here split the node along the x direction
                angtan_distribution(list_left, list_right, size_list_l, size_list_r, l, ll);
            }
        }
    }
    lwfree(list_left);
    lwfree(list_right);
    lwfree(list_bottom);
    lwfree(list_top);
}
