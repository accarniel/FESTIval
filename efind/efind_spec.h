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
 * File:   efind_spec.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 14, 2017, 1:09 AM
 */

#ifndef EFIND_SPEC_H
#define EFIND_SPEC_H

#include <stdint.h>
#include <stddef.h>

/*This file defines the eFINDSpecification since it was employing an auto relationship with r-tree and others*/

/* THE SPECIFICATION OF eFIND indices
 * NOTE: It does not have source, generic parameters since it extends a disk-based index (like the R-tree) */
typedef struct {
    size_t write_buffer_size; //buffer_size to be used for the write_buffer
    size_t read_buffer_size; //a buffer_size to be used for the read buffer with the HLRU policy
    
    uint8_t temporal_control_policy; //the policy of the temporal control (see the efind.h file)
    
    double read_temporal_control_perc; //the percentage usage to determine the size of the temporal control for reads
    
    int write_temporal_control_size; //the size of the temporal control for writes (in general, this values is the same of the flushing unit size)
    int write_tc_minimum_distance;
    int write_tc_stride;    
    
    double timestamp_perc; //a percentage value to be used in the flushing operation
    
    int flushing_unit_size; //size of a flushing unit (the number of pages/nodes)
    uint8_t flushing_policy; //see efind.h    
    uint8_t read_buffer_policy; //see efind.h
    void *rbp_additional_params; //see efind.h
    size_t log_size; //maximum size of the log
    char *log_file; //the path to the log file
    int index_sc_id; //only used for statistical processing - it corresponds to the index_sc_id of the FESTIval data schema
    double read_buffer_perc; //only used for statistical processing
    
    //some needed info
    size_t offset_last_elem_log; //the offset of the last element of the log file
    size_t size_last_elem_log; //the size of the last element of the log file    
} eFINDSpecification;

#endif /* EFIND_SPEC_H */

