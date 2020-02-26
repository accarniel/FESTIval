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
 * File:   hilbert_node.h
 * Author: anderson
 *
 * Created on November 21, 2017, 11:03 AM
 */

#ifndef HILBERT_NODE_H
#define HILBERT_NODE_H

#include "../main/spatial_index.h"      //for GenericParameters
#include "../main/bbox_handler.h"       //for BBOX

/* Hilbert R-tree is based on the R-tree, therefore, 
 * we will use several structures of the R-tree implementation*/

#include "../rtree/rtree.h"             //for RNode

typedef unsigned long long hilbert_value_t;

/*definition of an entry of a internal node of the hilbert r-tree*/
typedef struct {
    int pointer; //which can be to the object (leaf) or to the child node (internal)
    BBox *bbox; //the bbox of the element
    hilbert_value_t lhv; //LHV is the largest Hilbert value among the data rectangles enclosed by bbox.
} HilbertIEntry;

#define HILBERT_INTERNAL_NODE   1
#define HILBERT_LEAF_NODE       2

/*if the node is in the height equal to 0, then it is a leaf node*/
typedef struct {
    int nofentries; //number of entries
    uint8_t type; //is it a non-leaf or leaf node? see the macros above
    union {
        REntry **leaf; //array of entries if type is LEAF
        HilbertIEntry **internal; //array of entries if type is INTERNAL
    } entries;   
} HilbertRNode;

/*append an entry into a node respecting the order of the hilbert value (it does not copy the entry, it only add its reference)
 it returns the position in which the node was inserted*/
extern int hilbertnode_add_entry(HilbertRNode *node, void *entry, hilbert_value_t h, int srid);

/*remove an entry from a node*/
extern void hilbertnode_remove_entry(HilbertRNode *node, int entry);

/* copy an internal entry and return its pointer */
extern HilbertIEntry *hilbertientry_clone(const HilbertIEntry *entry);

/* copy and node and return its pointer */
extern HilbertRNode *hilbertnode_clone(const HilbertRNode *node);

/* copy in a destination node a node*/
extern void hilbertnode_copy(HilbertRNode *dest, const HilbertRNode *src);

/* create an empty Hilbert Node*/
extern HilbertRNode *hilbertnode_create_empty(uint8_t type);

/* return the size of a node instance*/
extern size_t hilbertnode_size(const HilbertRNode *node);

/* return the size of an internal entry */
extern size_t hilbertientry_size(void);

/* free a HilbertRNode */
extern void hilbertnode_free(HilbertRNode *node);

/* free an entry of a HilbertRNode */
extern void hilbertentry_free(HilbertIEntry *entry);

/* create an internal entry */
extern HilbertIEntry *hilbertentry_create(int pointer, BBox *bbox, hilbert_value_t lhv);

/* read the node from file */
extern HilbertRNode *get_hilbertnode(const SpatialIndex *si, int page_num, int height);

/* write the node to file */
extern void put_hilbertnode(const SpatialIndex *si, const HilbertRNode *node, int page_num, int height);

/* delete a node from file */
extern void del_hilbertnode(const SpatialIndex *si, int page_num, int height);

/* serialize a node*/
extern void hilbertnode_serialize(const HilbertRNode *node, uint8_t *buf);

/*set the coordinates of a bbox by considering a set of entries (union of all these entries)
 it also returns its LHV, largest hilbert value*/
extern hilbert_value_t hilbertnode_compute_bbox(const HilbertRNode *node, int srid, BBox *un);

/*it calculates the hilbert value of a bbox*/
extern hilbert_value_t hilbertvalue_compute(const BBox *bbox, int srid);

/*it sort the entries of a hilbert node according to their hilbert values*/
extern void hilbertnode_sort_entries(HilbertRNode *node, int srid);

/*compute the dead space area of a hilbertnode */
extern double hilbertnode_dead_space_area(const HilbertRNode *node, int srid);

/* compute the overlapping area of a hilbertnode */
extern double hilbertnode_overlapping_area(const HilbertRNode *node);

/* compute the overlapping area of a set of HilbertIEntry entries */
extern double hilbertientries_overlapping_area(const HilbertIEntry **entries, int n);

/*it shows in the standard output of the postgresql a RNODE - only for debug modes*/
extern void hilbertnode_print(const HilbertRNode *node, int node_id);

#endif /* HILBERT_NODE_H */

