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
 * File:   fornode_stack.h
 * Author: Anderson Chaves Carniel
 *
 * Created on April 13, 2016, 12:54 AM
 */

#ifndef FORNODE_STACK_H
#define FORNODE_STACK_H

#include "../rtree/rnode.h"
#include "fortree_nodeset.h"

typedef struct _fornode_stack_item {
    RNode *parent;
    int parent_add;
    int entry_of_parent;
    bool parent_is_onode;
    RNode *p_node_of_parent;
    int p_node_add;
    FORNodeSet *s;
    struct _fornode_stack_item *next;
} FORNodeStackItem;

typedef struct {
    FORNodeStackItem *top;
    int size;
} FORNodeStack;

extern FORNodeStack *fornode_stack_init(void);

extern void fornode_stack_push(FORNodeStack *stack, RNode *p, int parent_add, int entry_of_p, bool is_onode, RNode *p_node, int p_node_add, FORNodeSet *s);

extern RNode *fornode_stack_pop(FORNodeStack *stack, int *parent_add, int *entry_of_p, bool *is_onode, RNode **p_node, int *p_node_add, FORNodeSet **s);

extern void fornode_stack_pop_without_return(FORNodeStack *stack);

extern RNode *fornode_stack_peek(FORNodeStack *stack, int *parent_add, int *entry_of_p, bool *is_onode, RNode **p_node, int *p_node_add, FORNodeSet **s);

/* free the stack from memory*/
extern void fornode_stack_destroy(FORNodeStack *stack);

#endif /* _FORNODE_STACK_H */

