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
 * File:   efind_log.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 14, 2017, 8:26 PM
 */

#ifndef EFIND_LOG_H
#define EFIND_LOG_H

#include "../rtree/rnode.h"
#include "../main/bbox_handler.h"
#include "efind.h"

#define eFIND_STATUS_FLUSH   4

typedef struct {
    void *entry; //which is e.g. a REntry or Hilbert REntry
} eFINDLogValue;

typedef struct {
    int n;
    int *pages_id;
} eFINDFlushedNodes;

/*this is the structure of an entry stored in the log
 Before every entry, we have the size in bytes of the previous element
 this is useful since we traverse the log file in the reserve order!*/
typedef struct {
    int page_id; //it has only a valid value for NEW, MOD, and DEL
    
    int height; //it is the height of the node only for NEW, MOD, and DEL
    
    uint8_t status;

    union {
        eFINDLogValue *mod; //this is a valid value only if status is equal to MOD or NEW
        eFINDFlushedNodes *flushed_nodes; //this is a valid value only if status is equal to FLUSH
    } value;
} eFINDLogEntry;

extern void efind_write_log_create_node(const SpatialIndex *base, eFINDSpecification *spec,
        int new_node_page, int height);
extern void efind_write_log_mod_node(const SpatialIndex *base, eFINDSpecification *spec, 
        int node_page, void *entry, int height);
extern void efind_write_log_del_node(const SpatialIndex *base, eFINDSpecification *spec,
        int node_page, int height);

extern void efind_write_log_flush(const SpatialIndex *base, eFINDSpecification *spec,
        int* flushed_nodes, int n);

extern void efind_compact_log(const SpatialIndex *base, eFINDSpecification *spec);
extern void efind_recovery_log(const SpatialIndex *base, eFINDSpecification *spec);

#endif /* FASINF_LOG_H */

