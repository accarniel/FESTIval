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

#include "rnode_stack.h"
#include "../main/log_messages.h"

RNodeStack *rnode_stack_init() {
    RNodeStack *stack = (RNodeStack*)lwalloc(sizeof(RNodeStack));
    stack->top = NULL;
    stack->size = 0;
    return stack;
}

/* insert in the first */
void rnode_stack_push(RNodeStack *stack, RNode *p, int parent_add, int entry_of_p) {
    RNodeStackItem *tmp = (RNodeStackItem*) lwalloc(sizeof (RNodeStackItem));
    if (tmp == NULL) {
        _DEBUG(ERROR, "There is no memory to allocate in our stack of nodes");
    }
    tmp->parent = p;
    tmp->parent_add = parent_add;
    tmp->entry_of_parent = entry_of_p;
    tmp->next = stack->top;
    stack->top = tmp;
    stack->size++;
}

/* remove from the beginning */
RNode *rnode_stack_pop(RNodeStack *stack, int *parent_add, int *entry_of_p) {
    RNodeStackItem *it = stack->top;
    RNode *node;
    if (stack->top == NULL) {
        return NULL;
    }
    node = rnode_clone(it->parent);
    
    if(entry_of_p != NULL)
        *entry_of_p = it->entry_of_parent;
    if(parent_add != NULL)
        *parent_add = it->parent_add;

    stack->top = it->next;
    stack->size--;

    rnode_free(it->parent);
    lwfree(it);

    return node;
}

/* remove from the beginning, but without returning values */
void rnode_stack_pop_without_return(RNodeStack *stack) {
    RNodeStackItem *it = stack->top;
    if (stack->top == NULL) {
        return;
    }
    
    stack->top = it->next;
    stack->size--;

    rnode_free(it->parent);
    lwfree(it);
}

/* just return the reference and do not remove it */
RNode *rnode_stack_peek(RNodeStack *stack, int *parent_add, int *entry_of_p) {
    RNodeStackItem *it = stack->top;
    RNode *node = it->parent;
    if (stack->top == NULL) {
        return NULL;
    }
    
    if(entry_of_p != NULL)
        *entry_of_p = it->entry_of_parent;
    if(parent_add != NULL)
        *parent_add = it->parent_add;

    return node;
}

void rnode_stack_destroy(RNodeStack *stack) {
    RNodeStackItem *current;
    while (stack->top != NULL) {
        current = stack->top;
        stack->top = current->next;

        rnode_free(current->parent);
        lwfree(current);
    }
    lwfree(stack);
}
