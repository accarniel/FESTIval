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
 * File:   node_stack.h
 * Author: Anderson Chaves Carniel
 *
 * Created on November 30, 2017, 11:19 AM
 */

#ifndef HILBERTNODE_STACK_H
#define HILBERTNODE_STACK_H

#include "hilbert_node.h"

typedef struct _hilbertnode_stack_item {
    HilbertRNode *parent;
    int parent_add;
    int entry_of_parent;
    struct _hilbertnode_stack_item *next;
} HilbertRNodeStackItem;

typedef struct {
    HilbertRNodeStackItem *top;
    int size;
} HilbertRNodeStack;

extern HilbertRNodeStack *hilbertnode_stack_init(void);

extern void hilbertnode_stack_push(HilbertRNodeStack *stack, HilbertRNode *p, int parent_add, int entry_of_p);

/* this function returns a copy of the HilbertRNODE and then destroy it from the stack
 it also copy the entry_of_p passed as reference*/
extern HilbertRNode *hilbertnode_stack_pop(HilbertRNodeStack *stack, int *parent_add, int *entry_of_p);

 /* remove from the beginning, but without returning values */
extern void hilbertnode_stack_pop_without_return(HilbertRNodeStack *stack);

extern HilbertRNode *hilbertnode_stack_peek(HilbertRNodeStack *stack, int *parent_add, int *entry_of_p);

/* free the stack*/
extern void hilbertnode_stack_destroy(HilbertRNodeStack *stack);


#endif /* HILBERTNODE_STACK_H */

