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
 *
 **********************************************************************/

/* this file calculates the hilbert value using the implementation done by
 * 
 * Author:      Doug Moore
 *              Dept. of Computational and Applied Math
 *              Rice University
 *              http://www.caam.rice.edu/~dougm
 * Date:        Sun Feb 20 2000
 * Copyright (c) 1998-2000, Rice University 
 * (see hilbert_curve.c and hilbert_curve.h files)
 */

#include <math.h>
#include <string.h>
#include "hilbert_node.h"
#include "hilbert_curve.h"
#include "hilbert_value.h"

#include "../main/log_messages.h"

/*
normalize coordinates (x, y) to an hilbert_value_t, which is accepted as input to calculate its index in the hilbert curve
 * it returns an array of two elements
 */
static hilbert_value_t *normalize(double x, double y, int srid) {
    hilbert_value_t *res = (hilbert_value_t*) malloc(sizeof (hilbert_value_t)*2);
    //The precision has the resolution of the domain that you will transform
    hilbert_value_t precision;
    hilbert_value_t newRange;
    double oldRangeX, oldRangeY;

    precision = pow(2, RESOLUTION);
    
    newRange = precision - 1;
    //newmax is precision and newmin is 1
    /*
    OldRange = (OldMax - OldMin)  
    NewRange = (NewMax - NewMin)  
    NewValue = (((OldValue - OldMin) * NewRange) / OldRange) + NewMin
     */

    //transform in a hilbert_value_t according to an SRID value
    switch (srid) {
        case 0: //in the most of the cases, the SRID equal to 0 refers to the 4326
        case 4326:
        {
            oldRangeX = SRID_4326_MaxX - SRID_4326_MinX;
            oldRangeY = SRID_4326_MaxY - SRID_4326_MinY;
            res[0] = (hilbert_value_t) ((x - SRID_4326_MinX) * newRange) / oldRangeX;
            res[1] = (hilbert_value_t) ((y - SRID_4326_MinY) * newRange) / oldRangeY;
            break;
        }
        case 3857:
        {
            oldRangeX = SRID_3857_MaxX - SRID_3857_MinX;
            oldRangeY = SRID_3857_MaxY - SRID_3857_MinY;
            res[0] = (hilbert_value_t) ((x - SRID_3857_MinX) * newRange) / oldRangeX;
            res[1] = (hilbert_value_t) ((y - SRID_3857_MinY) * newRange) / oldRangeY;
            break;
        }
        case 2029:
        {
            oldRangeX = SRID_2029_MaxX - SRID_2029_MinX;
            oldRangeY = SRID_2029_MaxY - SRID_2029_MinY;
            res[0] = (hilbert_value_t) ((x - SRID_2029_MinX) * newRange) / oldRangeX;
            res[1] = (hilbert_value_t) ((y - SRID_2029_MinY) * newRange) / oldRangeY;
            break;
        }
        default:
        {
            _DEBUGF(ERROR, "FESTIval does not support this SRID %d.", srid);
        }
    }
    return res;
}

/*return the hilbert value from a given coordinate (x, y) point*/
hilbert_value_t calculate_hilbert_value(double x, double y, int srid) {
    hilbert_value_t *normalized; /*the normalized values of x and y */

    hilbert_value_t ret;

    normalized = normalize(x, y, srid);

    //number of dimensions here is 2
    ret = hilbert_c2i(2, RESOLUTION, (const bitmask_t*) normalized);
    free(normalized);

    return ret;
}
