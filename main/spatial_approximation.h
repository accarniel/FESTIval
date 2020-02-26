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
 * File:   spatial_approximation.h
 * Author: Anderson Chaves Carniel
 *
 * Created on November 20, 2016, 5:45 PM
 */

#ifndef SPATIAL_APROXIMATION_H
#define SPATIAL_APROXIMATION_H

#define NUM_OF_DIM      2 //the default is 2, make possible the changing after (with autoconf)
#define MAX_DIM         ((NUM_OF_DIM)-1)

/*
 * TYPES OF RELATIONSHIPS
 */
#define INTERSECTS              1
#define OVERLAP                 2
#define DISJOINT                3
#define MEET                    4
#define INSIDE                  5 //it also corresponds to contains (only change the order of the objects)
#define COVEREDBY               6 //it also corresponds to covers (only change the order of the objects)
#define CONTAINS                7
#define COVERS                  8
#define EQUAL                   9
#define INSIDE_OR_COVEREDBY     10 //it corresponds to inside OR coveredBy (for containment check)
#define CONTAINS_OR_COVERS      11 //it corresponds to CONTAINS OR COVERS (for containment check)

/*it will include other generic definitions: such as the type of the approximation,
 a generic structure to represent an approximation (for a intermediary step of processing)*/

#endif /* _SPATIAL_APROXIMATION_H */

