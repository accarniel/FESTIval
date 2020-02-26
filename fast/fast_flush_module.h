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
 * File:   fast_flush_module.h
 * Author: Anderson Chaves Carniel
 *
 * Created on March 31, 2016, 8:03 PM
 */

#ifndef FAST_FLUSH_MODULE_H
#define FAST_FLUSH_MODULE_H

#include "fast_index.h"

/*flushing unit for FAST!*/
typedef struct {
    int *node_pages; //this is an array of rnode pages.
	//TODO resolve o problema da funcao fb_get_node_height de fast_buffer aqui! para isso armazena-se a altura dos nos aqui tb como um outro array.
    int n; //how many pages were allocated?
} FASTFlushingUnit;

/*this function add a node into a flushing unit
 a flushing unit is a set of X nodes (specified in the FAST_SPECIFICATION)
 * if the last flushing unit has space, then we add it into the last flushing unit
 
 * NOTEs: 
 * (1) if the flushing policy is FAST_FF then 
 * we also increment the number of modifications for the corresponding flushing unit in 1
 * for it we get the flushing unit that rnode_page belongs to.
 * (2) if the flushing policy is FAST_STAR then
 * we also increment the number of modifications for the corresponding flushing unit in 1
 * and we update the timestamp of the flushing unit
 * that indicates the last time this flushing unit has been updated
 * (3) if the flushing policy is RANDOM or FLUSH_ALL then
 * we only form the flushing unit by grouping rnodes... like described above
 */
extern void fast_set_flushing_unit(const FASTSpecification *spec, int rnode_page);

/*note that the size of freed bytes of our buffer is different from the flushing unit size
 for instance, the flushing unit is 16KB in most SSD
 it would correspond to 4 node pages with 4KB
 * HOWEVER, this function does not free 16kb of our buffer!
 the reason is that the modifications stored in our buffer may not be 4KB for each node
 since we store only the modifications of a node and not the complete node with size equal to 4KB
 even for newly created nodes, we do not consider 4KB - see rnode_size function*/
extern void fast_execute_flushing(const SpatialIndex *base, FASTSpecification *spec);

/*this functions will apply ALL the existing flushing units!*/
extern void fast_flush_all(const SpatialIndex *base, FASTSpecification *spec);

extern void fast_destroy_flushing(void);

#endif /* _FAST_FLUSH_MODULE_H */

