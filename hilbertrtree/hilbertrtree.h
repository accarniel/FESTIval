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
 * File:   hilbertrtree.h
 * Author: Anderson Chaves Carniel
 *
 * Created on November 21, 2017, 12:13 AM
 */

#ifndef HILBERTRTREE_H
#define HILBERTRTREE_H

/* This file specifies the Hilbert R-tree index according to the original article
 * Reference: KAMEL, I.; FALOUTSOS, C. Hilbert R-tree: An Improved R-tree Using Fractals. 
 * Proceedings of the VLDB Conference, p. 500-509, 1994.
 */

#include "hilbert_node.h"


/* THE SPECIFICATION OF Hilbert R-tree
 i.e., the parameters that a Hilbert R-tree can has */
typedef struct {
    int or_id; //in order to know the occupancy rate used since it has the percentage format!
    /* occupancy information*/
    int max_entries_int_node; //number of M (i.e., the total entries allowed for an internal node)
    int max_entries_leaf_node; //number of M (i.e., the total entries allowed for an leaf node)
    int min_entries_int_node; //number of m (i.e., the minimum entries allowed for an internal node)
    int min_entries_leaf_node; //number of m (i.e., the minimum entries allowed for an leaf node)

    /*specific parameters of the Hilbert R-tree*/
    int order_splitting_policy;
    /*this parameter determines the policy of the split
     * s-to-(s+1) split
     * */  
    int srid; //it specifies the SRID used to create the hilbert r-tree, we need it because of the calculation of hilbert values
} HilbertRTreeSpecification;

/* THE DEFINITION OF A HILBERT R*-TREE INDEX as a subtype of spatialindex */
typedef struct {    
    SpatialIndex base; //it includes the source, generic parameters, and general functions
    //internal control (e.g., statistical data and to write data blocks)
    uint8_t type; //type here can be CONVENTIONAL_HILBERTRTREE or FAST_HILBERTRTREE, and so on

    HilbertRTreeSpecification *spec; //the parameters/specification of this index
    RTreesInfo *info; //information about this index
    HilbertRNode *current_node; //what is the current rnode of this index?
} HilbertRTree;


/* return a new (empty) Hilbert R-tree index, it only specifies the general parameters but not the specific parameters! */
extern SpatialIndex *hilbertrtree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, bool persist);

/* for FAST Hilbert R-tree indices we have to specify this parameter */
extern void hilbertrtree_set_fastspecification(FASTSpecification *fesp);

/*for eFIND Hilbert R-tree indices we have to specify this parameter*/
extern void hilbertrtree_set_efindspecification(eFINDSpecification *fesp);

#endif /* HILBERTRTREE_H */

