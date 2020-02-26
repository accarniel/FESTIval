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
 * File:   fast_spec.h
 * Author: Anderson Chaves Carniel
 *
 * Created on October 21, 2016, 4:27 PM
 */

#ifndef FAST_SPEC_H
#define FAST_SPEC_H

#include <stdint.h>
#include <stddef.h>

/*This file defines the FASTSpecification since it was employing an auto relationship with r-tree and others*/

/* THE SPECIFICATION OF FAST indices
 i.e., the parameters that a FAST can has 
 * NOTE: It does not have source, generic parameters since it extends a disk-based index (like the R-tree) */
typedef struct {
    size_t buffer_size; //size of the buffer in bytes
    int flushing_unit_size; //size of a flushing unit (the number of pages/nodes)
    uint8_t flushing_policy; //see above        
    size_t log_size; //maximum size of the log
    char *log_file; //the path to the log file
    int index_sc_id; //only used for statistical processing - it corresponds to the index_sc_id of the FESTIval data schema
    
    //some needed info
    size_t offset_last_elem_log; //the offset of the last element of the log file
    size_t size_last_elem_log; //the size of the last element of the log file    
} FASTSpecification;

#endif /* FAST_SPEC_H */

