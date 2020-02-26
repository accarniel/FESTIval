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
 * File:   rnode.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 29, 2016, 4:21 PM
 * Updated on September 24, 2016
 */

#ifndef RNODE_H
#define RNODE_H

#include "../main/spatial_index.h"      //for GenericParameters
#include "../main/bbox_handler.h"       //for BBOX


/* this file defines the basic structure used by indices based on the R-tree
 * RNode is used by R-tree and R*-tree
 * REntry is a entry of the R-tree and R*-tree
 */

/*definition of an entry of a RNODE*/
typedef struct {
    int pointer; //which can be to the object (leaf) or to the child node (internal)
    BBox *bbox; //the bbox of the element
} REntry;

/*if the node is in the height equal to 0, then it is a leaf node*/
typedef struct {
    int nofentries; //number of entries
    REntry **entries; //array of entries
} RNode;

/*append an entry into a node (it does not copy the entry, it only add its reference)*/
extern void rnode_add_rentry(RNode *node, REntry *entry);

/*remove an entry from a node*/
extern void rnode_remove_rentry(RNode *node, int entry);

/* copy an entry and return its pointer */
extern REntry *rentry_clone(const REntry *entry);

/* copy and rnode and return its pointer */
extern RNode *rnode_clone(const RNode *rnode);

/* copy in a destination node a node*/
extern void rnode_copy(RNode *dest, const RNode *src);

/* create an empty RNODE*/
extern RNode *rnode_create_empty(void);

/* return the size of a node instance*/
extern size_t rnode_size(const RNode *node);

/* return the size of a rentry */
extern size_t rentry_size(void);

/* free a RNODE */
extern void rnode_free(RNode *node);

/* free a REntry*/
extern void rentry_free(REntry *entry);

/* compute the BBOX of a node (on all its entries) */
extern BBox *rnode_compute_bbox(const RNode *node);

/* create an entry */
extern REntry *rentry_create(int pointer, BBox *bbox);

/* read the node from file */
extern RNode *get_rnode(const SpatialIndex *si, int page_num, int height);

/* write the node to file */
extern void put_rnode(const SpatialIndex *si, const RNode *node, int page_num, int height);

/* delete a node from file */
extern void del_rnode(const SpatialIndex *si, int page_num, int height);

/* serialize a rnode*/
extern void rnode_serialize(const RNode *node, uint8_t *buf);

/*compute the dead space area of a rnode */
extern double rnode_dead_space_area(const RNode *node);

/* compute the overlapping area of a rnode */
extern double rnode_overlapping_area(const RNode *node);

/* compute the overlapping area of a set of entries */
extern double rentries_overlapping_area(const REntry **entries, int n);

/* compute the margin of a set of entries */
extern double rentry_margin(const REntry **entries, int n);

/*set the coordinates of a bbox by considering a set of entries (union of all these entries)*/
extern void rentry_create_bbox(const REntry **entries, int n, BBox *un);

/*it shows in the standard output of the postgresql a RNODE - only for debug modes*/
extern void rnode_print(const RNode *node, int node_id);

#endif /* _RNODE_H */

