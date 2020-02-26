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
 * File:   hilber_value.h
 * Author: anderson
 *
 * Created on November 22, 2017, 5:48 PM
 */

#ifndef HILBER_VALUE_H
#define HILBER_VALUE_H

#include "srid.h"
#include "../main/spatial_approximation.h"

#define RESOLUTION              ((8*sizeof(unsigned long long)) / NUM_OF_DIM) 

extern hilbert_value_t calculate_hilbert_value(double x, double y, int srid);

#endif /* HILBER_VALUE_H */

