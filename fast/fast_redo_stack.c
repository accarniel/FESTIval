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

#include "fast_redo_stack.h"
#include "../main/log_messages.h"
#include "fast_buffer.h"
#include "fast_buffer_list_mod.h"
#include "fast_log_module.h"
#include <liblwgeom.h> //for lwalloc

RedoStack *redostack_init() {
    RedoStack *stack = (RedoStack*) lwalloc(sizeof (RedoStack));
    stack->top = NULL;
    stack->size = 0;
    return stack;
}

void redostack_push(RedoStack *stack, LogEntry *l_entry) {
    RedoStackItem *tmp = (RedoStackItem*) lwalloc(sizeof (RedoStackItem));
    if (tmp == NULL) {
        _DEBUG(ERROR, "There is no memory to allocate in our stack of nodes");
    }
    tmp->l_entry = l_entry;
    tmp->next = stack->top;
    stack->top = tmp;
    stack->size++;
}

/* this function returns a copy of the LOG_ENTRY and then destroy it from the stack*/
LogEntry *redostack_pop(RedoStack *stack, uint8_t index_type) {
    RedoStackItem *it = stack->top;
    LogEntry *le;
    if (stack->top == NULL) {
        return NULL;
    }
    
    //this is what we will return
    le = it->l_entry;

    //the next top of the stack
    stack->top = it->next;
    stack->size--;
    
    //an upper free
    lwfree(it);

    return le;
}

/* free the stack*/
void redostack_destroy(RedoStack * stack, uint8_t index_type) {
    RedoStackItem *current;
    while (stack->top != NULL) {
        current = stack->top;
        stack->top = current->next;

        log_entry_free(current->l_entry, index_type);
        lwfree(current);
    }
    lwfree(stack);
}
