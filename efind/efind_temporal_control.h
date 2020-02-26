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
 * File:   efind_temporal_control.h
 * Author: anderson
 *
 * Created on September 5, 2017, 9:54 PM
 */

#ifndef EFIND_TEMPORAL_CONTROL_H
#define EFIND_TEMPORAL_CONTROL_H

#include "efind_flushing_manager.h"

//these macros are used below (see the temporal control functions)
#define INSERTED        1
#define NOT_INSERTED    0

/*the minimum number of elements considered in the list read_temporal_control*/
#define MINIMUM_READ_TEMPORAL_CONTROL_SIZE    10

/******************************
 * BASIC FUNCTION CALLED BY ALL READ BUFFER POLICIES
 *****************************/
extern void efind_add_read_temporal_control(const eFINDSpecification *spec, int node_id);
//it returns INSERTED if the p_id is contained in the temporal control for reads
//otherwise, it returned NOT_INSERTED
extern uint8_t efind_read_temporal_control_contains(const eFINDSpecification *spec, int p_id);
//it deletes an entry from the temporal control for reads (useful for the 2Q)
extern void efind_read_temporal_control_remove(const eFINDSpecification *spec, int page_id);

/******************************
 * BASIC FUNCTION CALLED BY THE FLUSHING OPERATION
 *****************************/
extern void efind_add_write_temporal_control(const eFINDSpecification *spec, int node_id);

//it returns INSERTED if it forced the insertion of a node in the read buffer, it returns NOT_INSERTED otherwise.
extern uint8_t efind_temporal_control_for_reads(const SpatialIndex *base, const eFINDSpecification *spec,
        int node_id, int node_height, void *node, uint8_t index_type);
//it returns a list of chosen pages that can be flushed, which were filtered according to old writes
extern ChosenPage *efind_temporal_control_for_writes(const eFINDSpecification *spec,
        const ChosenPage *raw, int n, int *n_ret);


extern void efind_temporal_control_destroy(void);

#endif /* EFIND_TEMPORAL_CONTROL_H */

