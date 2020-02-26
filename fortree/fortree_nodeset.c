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

#include "fortree_nodeset.h"
#include <liblwgeom.h>

void fortree_nodeset_destroy(FORNodeSet *s) {
    int i;
    if (s != NULL) {
        for (i = 0; i < s->n; i++) {
            if (s->o_nodes[i] != NULL)
                rnode_free(s->o_nodes[i]);
        }
        if (s->n > 0) {
            lwfree(s->o_nodes);
            lwfree(s->o_nodes_pages);
        }
        lwfree(s);
    }
}

/* this function is actually a clone operation */
void fortree_nodeset_copy(FORNodeSet *dest, const FORNodeSet *src) {
    int i;
    fortree_nodeset_destroy(dest);
    dest = fortree_nodeset_create(src->n);
    for (i = 0; i < src->n; i++) {
        dest->o_nodes[i] = rnode_clone(src->o_nodes[i]);
        dest->o_nodes_pages[i] = src->o_nodes_pages[i];
    }
}

FORNodeSet *fortree_nodeset_create(int size) {
    int i;
    FORNodeSet *s = (FORNodeSet*) lwalloc(sizeof (FORNodeSet));
    s->n = size;
    if (size > 0) {
        s->o_nodes = (RNode**) lwalloc(sizeof (RNode*) * s->n);
        s->o_nodes_pages = (int*) lwalloc(sizeof (int) * s->n);
    }
    for (i = 0; i < s->n; i++) {
        s->o_nodes[i] = NULL;
    }
    return s;
}

FORNodeSet *fortree_nodeset_clone(const FORNodeSet *src) {
    int i;
    if (src != NULL) {
        FORNodeSet *dest = fortree_nodeset_create(src->n);
        for (i = 0; i < src->n; i++) {
            dest->o_nodes[i] = rnode_clone(src->o_nodes[i]);
            dest->o_nodes_pages[i] = src->o_nodes_pages[i];
        }
        return dest;
    } else {
        return NULL;
    }
}
