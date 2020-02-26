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
 * Created on March 4, 2016, 11:11 PM
 */

#ifndef RNODE_STACK_H
#define RNODE_STACK_H

#include "rnode.h"

typedef struct _rnode_stack_item {
    RNode *parent;
    int parent_add;
    int entry_of_parent;
    struct _rnode_stack_item *next;
} RNodeStackItem;

typedef struct {
    RNodeStackItem *top;
    int size;
} RNodeStack;

extern RNodeStack *rnode_stack_init(void);

extern void rnode_stack_push(RNodeStack *stack, RNode *p, int parent_add, int entry_of_p);

/* this function returns a copy of the RNODE and then destroy it from the stack
 it also copy the entry_of_p passed as reference*/
extern RNode *rnode_stack_pop(RNodeStack *stack, int *parent_add, int *entry_of_p);

 /* remove from the beginning, but without returning values */
extern void rnode_stack_pop_without_return(RNodeStack *stack);

extern RNode *rnode_stack_peek(RNodeStack *stack, int *parent_add, int *entry_of_p);

/* free the stack*/
extern void rnode_stack_destroy(RNodeStack *stack);


#endif /* _RNODE_STACK_H */

