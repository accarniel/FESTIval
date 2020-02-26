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
 * File:   fast_buffer.h
 * Author: Anderson Chaves Carniel
 *
 * Created on March 30, 2016, 4:13 PM
 */

#ifndef FAST_BUFFER_H
#define FAST_BUFFER_H

#include "fast_index.h"

#define FAST_STATUS_NEW     1
#define FAST_STATUS_MOD     2
#define FAST_STATUS_DEL     3

/*we put a new NODE In the buffer -> key equal to new_node_page and the value is (NEW, a pointer to the new_node)
 if we already have a key equal to new_node_page, then we modify its value to new_node*/
extern void fb_put_new_node(const SpatialIndex *base, FASTSpecification *spec, 
        int new_node_page, void *new_node, int height);
/*we put a new RENTRY In the buffer -> key equal to rnode_page and the value is MOD and a triple:
 * (K, position, new_bbox)
 if we already have a key equal to rnode_page, then we append this modification*/
extern void fb_put_mod_bbox(const SpatialIndex *base, FASTSpecification *spec, 
        int node_page, BBox *new_bbox, int position, int height);
/*we put a new pointer In the buffer -> key equal to rnode_page and the value is MOD and a triple:
 * (P, position, new_pointer)
 if we already have a key equal to rnode_page, then we append this modification*/
extern void fb_put_mod_pointer(const SpatialIndex *base, FASTSpecification *spec, 
        int node_page, int new_pointer, int position, int height);
/*we put a NULL pointer in the buffer -> key equal to node_page and the value is DEL and a value NULL*/
extern void fb_del_node(const SpatialIndex *base, FASTSpecification *spec, 
        int node_page, int height);
/*the next 3 functions are extensions to deal with the Hilbert R-Tree*/
extern void fb_put_mod_lhv(const SpatialIndex *base, FASTSpecification *spec, 
        int node_page, hilbert_value_t new_lhv, int position, int height);
extern void fb_put_mod_hole(const SpatialIndex *base, FASTSpecification *spec, 
        int node_page, int position, int height);
/*this function is called when a new insertion is made in a HilbertNode!
 it is needed because an entry can be inserted in any position of a HilbertNode.*/
extern void fb_completed_insertion(void);
extern bool is_processing_hole(void);

/*we retrieve the most recent version of a RNODE by considering possible modification in the buffer
 after the call, we return the most recent version of the request node (rnode_page)*/
extern void *fb_retrieve_node(const SpatialIndex *base, int node_page, int height);

/*we definitely remove a node from the buffer*/
extern void fb_free_hashvalue(int node_page, uint8_t index_type);

extern int fb_get_nofmod(int node_page);

extern int fb_get_node_height(int node_page);

extern void fb_destroy_buffer(uint8_t index_type);

#endif /* _FAST_BUFFER_H */

