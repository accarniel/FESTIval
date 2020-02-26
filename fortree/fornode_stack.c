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

#include "fornode_stack.h"
#include "../main/log_messages.h"
#include <liblwgeom.h>

FORNodeStack *fornode_stack_init() {
    FORNodeStack *stack = (FORNodeStack*) lwalloc(sizeof (FORNodeStack));
    stack->top = NULL;
    stack->size = 0;
    return stack;
}

/* insert in the first */
void fornode_stack_push(FORNodeStack *stack, RNode *p, int parent_add, int entry_of_p, 
        bool is_onode, RNode *p_node, int p_node_add, FORNodeSet *s) {
    FORNodeStackItem *tmp = (FORNodeStackItem*) lwalloc(sizeof (FORNodeStackItem));
    if (tmp == NULL) {
        _DEBUG(ERROR, "There is no memory to allocate our stack of nodes");
    }
    tmp->parent = p;
    tmp->parent_add = parent_add;
    tmp->entry_of_parent = entry_of_p;
    tmp->parent_is_onode = is_onode;
    tmp->p_node_of_parent = p_node;
    tmp->p_node_add = p_node_add;
    tmp->s = s;
    tmp->next = stack->top;
    stack->top = tmp;
    stack->size++;
}

/* remove from the beginning - we set clones in the pointers */
RNode *fornode_stack_pop(FORNodeStack *stack, int *parent_add, int *entry_of_p,
        bool *is_onode, RNode **p_node, int *p_node_add, FORNodeSet **s) {
    FORNodeStackItem *it = stack->top;
    RNode *node;
    if (stack->top == NULL) {
        return NULL;
    }
    node = rnode_clone(it->parent);

    if (entry_of_p != NULL)
        *entry_of_p = it->entry_of_parent;
    if (parent_add != NULL)
        *parent_add = it->parent_add;
    if (is_onode != NULL)
        *is_onode = it->parent_is_onode;

    if (it->p_node_of_parent == NULL) {
        *p_node = rnode_create_empty();
    } else {
        *p_node = rnode_clone(it->p_node_of_parent);
    }

    if (p_node_add != NULL)
        *p_node_add = it->p_node_add;

    if (it->s == NULL) {
        *s = fortree_nodeset_create(0);
    } else {
        *s = fortree_nodeset_clone(it->s);
    }


    stack->top = it->next;
    stack->size--;

    rnode_free(it->parent);
    if (it->p_node_of_parent != NULL)
        rnode_free(it->p_node_of_parent);
    if (it->s != NULL)
        fortree_nodeset_destroy(it->s);
    lwfree(it);

    return node;
}

/* remove from the beginning, but without returning values */
void fornode_stack_pop_without_return(FORNodeStack *stack) {
    FORNodeStackItem *it = stack->top;
    if (stack->top == NULL) {
        return;
    }

    stack->top = it->next;
    stack->size--;

    rnode_free(it->parent);
    if (it->p_node_of_parent != NULL)
        rnode_free(it->p_node_of_parent);
    if (it->s != NULL)
        fortree_nodeset_destroy(it->s);
    lwfree(it);
}

/* just return the reference and do not remove it */
RNode *fornode_stack_peek(FORNodeStack *stack, int *parent_add, int *entry_of_p, 
        bool *is_onode, RNode **p_node, int *p_node_add, FORNodeSet **s) {
    FORNodeStackItem *it = stack->top;
    RNode *node = it->parent;
    if (stack->top == NULL) {
        return NULL;
    }

    if (entry_of_p != NULL)
        *entry_of_p = it->entry_of_parent;
    if (parent_add != NULL)
        *parent_add = it->parent_add;
    if (is_onode != NULL)
        *is_onode = it->parent_is_onode;

    if (it->p_node_of_parent == NULL) {
        *p_node = rnode_create_empty();
    } else {
        *p_node = rnode_clone(it->p_node_of_parent);
    }

    if (p_node_add != NULL)
        *p_node_add = it->p_node_add;

    if (it->s == NULL) {
        *s = fortree_nodeset_create(0);
    } else {
        *s = fortree_nodeset_clone(it->s);
    }


    return node;
}

void fornode_stack_destroy(FORNodeStack *stack) {
    FORNodeStackItem *current;
    while (stack->top != NULL) {
        current = stack->top;
        stack->top = current->next;

        rnode_free(current->parent);
        if (current->p_node_of_parent != NULL)
            rnode_free(current->p_node_of_parent);
        if (current->s != NULL)
            fortree_nodeset_destroy(current->s);
        lwfree(current);
    }
    lwfree(stack);
}
