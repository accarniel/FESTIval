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
 * File:   rstartree.h
 * Author: Anderson Chaves Carniel
 *
 * Created on March 27, 2016, 6:08 PM
 */

#ifndef RSTARTREE_H
#define RSTARTREE_H

/* This file specifies the R*-tree index according to the original article from Beckman
 * Reference: BECKMANN, N.; KRIEGEL, H.-P.; SCHNEIDER, R.; SEEGER, B. The R*-tree: An efficient
and robust access method for points and rectangles. SIGMOD Record, ACM, v. 19, n. 2, p.
322–331, 1990.
 */

/* R*-tree is based on the R-tree, therefore, 
 * we will use several files from the R-tree implementation*/

#include "../rtree/rtree.h"


/* THE SPECIFICATION OF R*-TREE 
 i.e., the parameters that a R*-tree can has */
typedef struct {
    int or_id; //in order to know the occupancy rate used since it has the percentage format!
    /* occupancy information*/
    int max_entries_int_node; //number of M (i.e., the total entries allowed for an internal node)
    int max_entries_leaf_node; //number of M (i.e., the total entries allowed for an leaf node)
    int min_entries_int_node; //number of m (i.e., the minimum entries allowed for an internal node)
    int min_entries_leaf_node; //number of m (i.e., the minimum entries allowed for an leaf node)

    /*specific parameters of the R-tree*/
    double reinsert_perc_internal_node; //the percentage to be considered in the reinsertion policy (only for internal nodes)
    double reinsert_perc_leaf_node; //the percentage to be considered in the reinsertion policy (only for leaf nodes)
    uint8_t reinsert_type; //(see above)
    int max_neighbors_to_examine; //parameter considered in the insertion routine    
} RStarTreeSpecification;

/* THE DEFINITION OF A R*-TREE INDEX as a subtype of spatialindex */
typedef struct {
    SpatialIndex base; //it includes the source, generic parameters, and general functions
    //internal control (e.g., statistical data and to write data blocks)
    uint8_t type; //type here can be CONVENTIONAL_RSTARTREE or FAST_RSTARTREE (see fast files)  

    RStarTreeSpecification *spec; //the parameters/specification of this index
    RTreesInfo *info; //information about this index
    RNode *current_node; //what is the current rnode of this index?
    bool *reinsert; //an array indicating if a height (the index of the array) have reinserted entries
} RStarTree;


/* return a new (empty) R*-tree index, it only specifies the general parameters but not the specific parameters! */
extern SpatialIndex *rstartree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, bool persist);

/* for FAST R*-tree indices we have to specify this parameter */
extern void rstartree_set_fastspecification(FASTSpecification *fesp);

/*for FASINF R*-tree indices we have to specify this parameter*/
extern void rstartree_set_efindspecification(eFINDSpecification *fesp);

/* an auxiliary function to convert a RStarTree to a RTree index since we reuse some algorithms of RTREE*/
extern RTree *rstartree_to_rtree(RStarTree *rstar);
/* an auxiliary function to free an converted RTree index (THIS IS ONLY VALID HERE!)*/
extern void free_converted_rtree(RTree *rtree);

#endif /* _RSTARTREE_H */

