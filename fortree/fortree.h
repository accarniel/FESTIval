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
 * File:   fortree.h
 * Author: Anderson Chaves Carniel
 *
 * Created on April 11, 2016, 1:38 PM
 * Updated on December 01, 2016
 */

/*
 * 
 * IMPORTANT: It strictly follows the FOR-tree original paper:
 * Reference: JIN, P.; XIE, X.; WANG, N.; YUE, L. Optimizing r-tree for flash memory. Expert Systems with
Applications, v. 42, n. 10, p. 4676–4686, 2015.
 */

#ifndef FORTREE_H
#define FORTREE_H

#include "../main/spatial_index.h" //this is a sub type of spatial_index
#include "../rtree/rnode.h"

/* THE SPECIFICATION OF FOR-TREE 
 i.e., the parameters that a FOR-tree can has */
typedef struct {
    int or_id; //in order to know the occupancy rate used since it has the percentage format!
    /* occupancy information*/
    int max_entries_int_node; //number of M (i.e., the total entries allowed for an internal node)
    int max_entries_leaf_node; //number of M (i.e., the total entries allowed for an leaf node)
    int min_entries_int_node; //number of m (i.e., the minimum entries allowed for an internal node)
    int min_entries_leaf_node; //number of m (i.e., the minimum entries allowed for an leaf node)

    /*specific parameters of the FOR-tree*/
    size_t buffer_size; //size of the main buffer of FOR-tree
    int flushing_unit_size; //number of nodes to be flushed in flushing operations
    double ratio_flushing; //this is a percentage value from 0 to 100
    double x; //the cost/time of a flash read operation
    double y; //the cost/time of a flash write operation
} FORTreeSpecification;

/* THE DEFINITION OF A FOR-TREE INDEX */
typedef struct {
    SpatialIndex base; //it includes the source, generic parameters, and general functions
    //internal control (e.g., statistical data and to write data blocks)
    uint8_t type; //type here is FORTREE_TYPE

    FORTreeSpecification *spec; //the parameters/specification that this index has
    RTreesInfo *info; //information about this index (see above)
    RNode *current_node; //what is the current rnode of this index?
} FORTree;

/* return a new (empty) FOR-tree index, it only specifies the general parameters but not the specific parameters! */
extern SpatialIndex *fortree_empty_create(char *file, Source *src, GenericParameters *gp,
        BufferSpecification *bs, FORTreeSpecification *spec, bool persist);

extern SpatialIndexResult *search_fortree(FORTree *fr, const BBox *query, uint8_t predicate);

extern void insert_fortree(FORTree *fr, REntry *input);

extern bool fortree_delete_entry(FORTree *fr, REntry *to_remove);

extern bool update_fortree(FORTree *fr, REntry *old, REntry *to_update);

extern int fortree_get_nof_onodes(int n_page);

extern int fortree_get_onode(int n_page, int index);

#endif /* _FORTREE_H */

