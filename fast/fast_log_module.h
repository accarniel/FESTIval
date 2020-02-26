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
 * File:   fast_log_module.h
 * Author: Anderson Chaves Carniel
 *
 * Created on April 5, 2016, 2:41 PM
 */

#ifndef FAST_LOG_MODULE_H
#define FAST_LOG_MODULE_H

#include "fast_index.h"

#define FAST_STATUS_FLUSH   4

typedef struct {
    int position;
    uint8_t type; //FAST_ITEM_TYPE_K or FAST_ITEM_TYPE_P or FAST_ITEM_TYPE_L

    union {
        int pointer; //only if type is equal to P
        BBox *bbox; //only if type is equal to K
        hilbert_value_t lhv;
    } value;
} LogMOD;

typedef struct {
    int n;
    int *node_pages;
} FlushedNodes;

/*this is the structure of an entry stored in the log
 Before every entry, we have the size in bytes of the previous element
 this is useful since we traverse the log file in the reserve order!*/
typedef struct {
    int node_page; //it has only a valid value for NEW, MODE and DEL
    int node_height; //it is the height of the node
    uint8_t status;

    union {
        void *node; //this is a valid value only if status is equal to NEW
        LogMOD *mod; //this is a valid value only if status is equal to MODE
        FlushedNodes *flushed_nodes; //this is a valid value only if status is equal to FLUSH
    } value;
} LogEntry;

extern void write_log_new_node(const SpatialIndex *base, FASTSpecification *spec, 
        int new_node_page, void *new_node, int height);
extern void write_log_mod_bbox(const SpatialIndex *base, FASTSpecification *spec, 
        int node_page, BBox *new_bbox, int position, int height);
extern void write_log_mod_pointer(const SpatialIndex *base, FASTSpecification *spec, 
        int node_page, int new_pointer, int position, int height);
extern void write_log_mod_lhv(const SpatialIndex *base, FASTSpecification *spec, 
        int node_page, hilbert_value_t new_lhv, int position, int height);
extern void write_log_mod_hole(const SpatialIndex *base, FASTSpecification *spec,
        int node_page, int position, int height);
extern void write_log_del_node(const SpatialIndex *base, FASTSpecification *spec, 
        int node_page, int height);

extern void write_log_flush(const SpatialIndex *base, FASTSpecification *spec, int* flushed_nodes, int n);

extern void compact_fast_log(const SpatialIndex *base, FASTSpecification *spec);
extern void recovery_fast_log(const SpatialIndex *base, FASTSpecification *spec);

extern void log_entry_free(LogEntry *le, uint8_t index_type);

#endif /* _FAST_LOG_MODULE_H */

