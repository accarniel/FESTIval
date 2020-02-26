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
 * File:   fast_redo_stack.h
 * Author: Anderson Chaves Carniel
 *
 * Created on April 6, 2016, 9:12 AM
 */

#ifndef FAST_REDO_STACK_H
#define FAST_REDO_STACK_H

#include "fast_log_module.h"

typedef struct _redo_stack_item {
    LogEntry *l_entry;
    struct _redo_stack_item *next;
} RedoStackItem;

typedef struct {
    RedoStackItem *top;
    int size;
} RedoStack;

extern RedoStack *redostack_init(void);

extern void redostack_push(RedoStack *stack, LogEntry *l_entry);

/* this function returns a copy of the LOG_ENTRY and then destroy it from the stack*/
extern LogEntry *redostack_pop(RedoStack *stack, uint8_t index_type);

/* free the stack*/
extern void redostack_destroy(RedoStack *stack, uint8_t index_type);


#endif /* _FAST_REDO_STACK_H */

