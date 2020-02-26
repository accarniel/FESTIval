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

#include "hilbertnode_stack.h"
#include "../main/log_messages.h"

HilbertRNodeStack *hilbertnode_stack_init(void) {
    HilbertRNodeStack *stack = (HilbertRNodeStack*)lwalloc(sizeof(HilbertRNodeStack));
    stack->top = NULL;
    stack->size = 0;
    return stack;
}

void hilbertnode_stack_push(HilbertRNodeStack *stack, HilbertRNode *p, int parent_add, int entry_of_p) {
    HilbertRNodeStackItem *tmp = (HilbertRNodeStackItem*) lwalloc(sizeof (HilbertRNodeStackItem));
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

/* this function returns a copy of the HilbertRNODE and then destroy it from the stack
 it also copy the entry_of_p passed as reference*/
HilbertRNode *hilbertnode_stack_pop(HilbertRNodeStack *stack, int *parent_add, int *entry_of_p) {
    HilbertRNodeStackItem *it = stack->top;
    HilbertRNode *node;
    if (stack->top == NULL) {
        return NULL;
    }
    node = hilbertnode_clone(it->parent);
    
    if(entry_of_p != NULL)
        *entry_of_p = it->entry_of_parent;
    if(parent_add != NULL)
        *parent_add = it->parent_add;

    stack->top = it->next;
    stack->size--;

    hilbertnode_free(it->parent);
    lwfree(it);

    return node;
}

 /* remove from the beginning, but without returning values */
void hilbertnode_stack_pop_without_return(HilbertRNodeStack *stack) {
    HilbertRNodeStackItem *it = stack->top;
    if (stack->top == NULL) {
        return;
    }
    
    stack->top = it->next;
    stack->size--;

    hilbertnode_free(it->parent);
    lwfree(it);
}

HilbertRNode *hilbertnode_stack_peek(HilbertRNodeStack *stack, int *parent_add, int *entry_of_p) {
    HilbertRNodeStackItem *it = stack->top;
    HilbertRNode *node;
    if (stack->top == NULL) {
        //stack if empty, this means that there is no parent node
        return NULL;
    }
    
    node = it->parent;
    
    if(entry_of_p != NULL)
        *entry_of_p = it->entry_of_parent;
    if(parent_add != NULL)
        *parent_add = it->parent_add;

    return node;
}

/* free the stack*/
void hilbertnode_stack_destroy(HilbertRNodeStack *stack) {
    HilbertRNodeStackItem *current;
    while (stack->top != NULL) {
        current = stack->top;
        stack->top = current->next;

        hilbertnode_free(current->parent);
        lwfree(current);
    }
    lwfree(stack);
}
