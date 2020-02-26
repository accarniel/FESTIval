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
 * File:   rtree.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 27, 2016, 12:48 AM
 * Updated on September 24, 2016
 */

/* This file specifies the R-tree index according to the original article from Guttman
  * Reference: GUTTMAN, A. R-trees: A dynamic index structure for spatial searching. SIGMOD Record,
ACM, New York, NY, USA, v. 14, n. 2, p. 47–57, 1984. 
 */

#ifndef RTREE_H
#define RTREE_H

#include "../main/spatial_index.h" //this is a sub type of spatial_index
#include "rnode_stack.h" //for the stack of RNodes -- it already includes RNode

#include "../fast/fast_spec.h" //for FASTSpec

#include "../efind/efind_spec.h" //for eFINDSpec


/* THE SPECIFICATION OF R-TREE 
 i.e., the parameters that a R-tree can has*/
typedef struct {
    int or_id; //in order to know the occupancy rate used since it has the percentage format!
   
    /* occupancy information*/
    int max_entries_int_node; //number of M (i.e., the total entries allowed for an internal node)
    int max_entries_leaf_node; //number of M (i.e., the total entries allowed for a leaf node)
    int min_entries_int_node; //number of m (i.e., the minimum entries allowed for an internal node)
    int min_entries_leaf_node; //number of m (i.e., the minimum entries allowed for a leaf node)

    /*specific parameters of the R-tree*/
    uint8_t split_type; //type of split adopted for the R-tree (see above)
} RTreeSpecification;

/* THE DEFINITION OF A R-TREE INDEX as a TYPE of SpatialIndex*/
typedef struct {
    SpatialIndex base; //it includes the source, generic parameters, and general functions
    //internal control (e.g., statistical data and to write data blocks)
    uint8_t type; //type here can be CONVENTIONAL_RTREE or FAST_RTREE (see fast files)  

    RTreeSpecification *spec; //the parameters/specification of this index
    RTreesInfo *info; //information about this index
    RNode *current_node; //what is the current rnode of this index?
} RTree;

/* return a new (empty) R-tree index, it only specifies the general parameters but not the specific parameters! */
extern SpatialIndex *rtree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, bool persist);

/* original search algorithm from Rtree -> this is used for the R*-tree too
 * rtree is the RTREE
 * query will be bbox of the POSTGIS object
 * predicate is the topological predicate of the query (e.g., intersects, inside)
 * 
 * We also try to reduce the quantity of candidates
 * by using MBRs relationships, like defined in:
 * 
 * CLEMENTINI, E.; SHARMA, J.; EGENHOFER, M. J. Modelling topological spatial relations:
Strategies for query processing. Computers & Graphics, v. 18, n. 6, p. 815–822, 1994.
 *  */
extern SpatialIndexResult *rtree_search(RTree *rtree, const BBox *search, uint8_t predicate);

/* original deletion algorithm from R-tree (this is used for the R*-tree!)
 * It uses the find_leaf and condense_tree, which are static functions in rtree.c
 * it also set the stack removed_nodes with the removed nodes in the condense tree
 * set true or false in the reinsert to decide if this algorithm reinsert the removed_nodes
 * the caller is responsible to free the removed_nodes!
 */
extern bool rtree_remove_with_removed_nodes(RTree *rtree, const REntry *to_remove, 
        RNodeStack *removed_nodes, bool reinsert);

/* for FAST R-tree indices we have to specify this parameter */
extern void rtree_set_fastspecification(FASTSpecification *fesp);

/* for FASINF R-tree indices we have to specify this parameter */
extern void rtree_set_efindspecification(eFINDSpecification *fesp);

#endif	// _RTREE_H

