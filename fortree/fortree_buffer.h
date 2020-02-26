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
 * File:   fortree_buffer.h
 * Author: Anderson Chaves Carniel
 *
 * Created on April 11, 2016, 5:52 PM
 * Updated on October 15, 2016, 22:09 PM
 */

#ifndef FORTREE_BUFFER_H
#define FORTREE_BUFFER_H

#include "../fortree/fortree.h"

#define FORTREE_STATUS_NEW     1
#define FORTREE_STATUS_MOD     2
#define FORTREE_STATUS_DEL     3

/*only create a new rnode in the buffer - this node has no modifications!*/
extern void forb_create_new_rnode(const SpatialIndex *base, FORTreeSpecification *spec, 
        int new_node_page, int height);

/*put a modification of an existing node*/
extern void forb_put_mod_rnode(const SpatialIndex *base, FORTreeSpecification *spec, 
        int rnode_page, int position, REntry *entry, int height);

/*delete a rnode (which can be stored in the disk or not)*/
extern void forb_put_del_rnode(const SpatialIndex *base, FORTreeSpecification *spec, 
        int rnode_page, int height);

/*we retrieve the most recent version of a RNODE by considering possible modification in the buffer
 after the call, we return the most recent version of the request node (rnode_page)*/
extern RNode *forb_retrieve_rnode(const SpatialIndex *base, int rnode_page, int height);

/*we definitely remove a rnode from the buffer*/
extern void forb_free_hashvalue(const FORTreeSpecification *fr, int rnode_page);

/*destroy all the variables related to the buffer*/
extern void forb_destroy_buffer(void);

/*execute the flushing operation like defined in FOR-tree original paper*/
extern void forb_flushing(const SpatialIndex *base, FORTreeSpecification *spec);

/*flush all the operations stored in the buffer (this is useful when we will finish a transaction)*/
extern void forb_flushing_all(const SpatialIndex *base, FORTreeSpecification *spec);


#endif /* _FORTREE_BUFFER_H */

