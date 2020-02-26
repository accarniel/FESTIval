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
 * File:   efind_flushing_manager.h
 * Author: Anderson Chaves Carniel
 *
 * Created on May 3, 2017, 12:10 PM
 */

#ifndef EFIND_FLUSHING_MANAGER_H
#define EFIND_FLUSHING_MANAGER_H

#include "efind.h"

/*a special struct to hold the nodes that will be possible flushed*/
typedef struct {
    int page_id;
    int height;
    int nofmod;
    double area;
    double ov_area;
} ChosenPage;

typedef struct {
    int n; //number of nodes of this flushing unit
    int *pages; //the nodes that compose this flushing unit
    int *heights; //the heights of the nodes of this flushing unit
    double v; //the v value of this flushing unit
} eFINDFlushingUnit;

/*execute the flushing operation like defined in FOR-tree original paper*/
extern void efind_flushing(const SpatialIndex *base, eFINDSpecification *spec);

/*flush all the operations stored in the buffer (this is useful when we will finish a transaction)*/
extern void efind_flushing_all(const SpatialIndex *base, eFINDSpecification *spec);

#endif /* EFIND_FLUSHING_MANAGER_H */
