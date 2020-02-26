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
 * File:   util.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 26, 2016, 8:12 AM
 */

/*
 * GENERIC DEFINITIONS HERE
 */

#ifndef UTIL_H
#define UTIL_H

#include <math.h>
#include <float.h>

/*this function is mainly used for checking an intersection/overlapping situation */

#define DB_TOLERANCE            1.0E-05

#define DB_IS_ZERO(A)           (fabs(A) <= DB_TOLERANCE)
#define DB_IS_EQUAL(A,B)	(fabs((A) - (B)) <= DB_TOLERANCE)
#define DB_IS_NOT_EQUAL(A,B)    (fabs((A) - (B)) > DB_TOLERANCE)
#define DB_MAX(A, B)            (((A) > (B)) ? (A) : (B))
#define DB_MIN(A, B)            (((A) < (B)) ? (A) : (B))
#define DB_LT(A,B)              ((B) - (A) > DB_TOLERANCE)
#define DB_LE(A,B)              ((A) - (B) <= DB_TOLERANCE)
#define DB_GT(A,B)              ((A) - (B) > DB_TOLERANCE)
#define DB_GE(A,B)              ((B) - (A) <= DB_TOLERANCE)

#endif /* _UTIL_H */

