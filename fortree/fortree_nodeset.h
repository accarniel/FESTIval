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
 * File:   nodeset.h
 * Author: Anderson Chaves Carniel
 *
 * Created on April 13, 2016, 11:55 AM
 */

#ifndef FORTREENODESET_H
#define FORTREENODESET_H

#include "../rtree/rnode.h"

typedef struct {
    RNode **o_nodes; //an array of pointers for Node objects
    int *o_nodes_pages; //the corresponding page number from o_nodes
    int n; //number of o_nodes
} FORNodeSet;

extern void fortree_nodeset_copy(FORNodeSet *dest, const FORNodeSet *source);

extern FORNodeSet *fortree_nodeset_clone(const FORNodeSet *src);

extern FORNodeSet *fortree_nodeset_create(int size);

extern void fortree_nodeset_destroy(FORNodeSet *s);

#endif /* FORTREENODESET_H */

