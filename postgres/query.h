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
 * File:   query.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 27, 2016, 1:16 AM
 */

/*
 * This library is responsible to define the 
 * refinement and filtering step in a spatial query processing
 * 
 * refinement and filter steps are defined in:
 * 
 * GAEDE, V.; GÜNTHER, O. Multidimensional access methods. ACM Computing Surveys,
v. 30, n. 2, p. 170–231, 1998.
 * 
 */

#ifndef QUERY_H
#define QUERY_H

#include <liblwgeom.h>

#include "../main/bbox_handler.h"
#include "../main/spatial_index.h"

/*Types of queries */
#define GENERIC_SELECTION_QUERY_TYPE    1
#define RANGE_QUERY_TYPE                2
#define POINT_QUERY_TYPE                3

/*Types of processing of a spatial query*/
#define FILTER_AND_REFINEMENT_STEPS  1
#define ONLY_FILTER_STEP             2

typedef struct {
    int nofentries; //number of entries of the query
    int max; //maximum number of geoms and their respective identifier (row_id)
    int *row_id; //array of identifiers of the respective geoms
    LWGEOM **geoms;    //array of geoms
} QueryResult;

/*functions to manage QUERY_RESULT */
extern void query_result_free(QueryResult *qr, uint8_t processing_type);
extern QueryResult *create_empty_query_result(void);
extern QueryResult *create_query_result(int max_elements);

/*The caller is responsible to connect in SPI and finish it!*/

/* it defines how to compute the filter step 
 * (i.e., how to call the index in order to answer the possible above queries)
 * */
typedef SpatialIndexResult* (*filter_step_processor_ss)(SpatialIndex *si, LWGEOM *input, uint8_t predicate, uint8_t query_type);
typedef QueryResult* (*refinement_step_processor_ss)(SpatialIndexResult *candidates, Source *src, 
        GenericParameters *gp, LWGEOM *input, uint8_t predicate);

/*the user is able to change it for his/her own functions in order to process the filter and refinement steps for spatial selections*/
extern void query_set_processor_ss(filter_step_processor_ss f, refinement_step_processor_ss r);

/* we define the following query: spatial selection 
 * index = the spatial index 
 * input = the point/region/window query
 * predicate = the predicate to be considered
 * query_type = the type of the query to be processed (see above)
 * processing_type = which step of the query we will process (see above)
 */
QueryResult *process_spatial_selection(SpatialIndex *si, LWGEOM *input, 
        uint8_t predicate, uint8_t query_type, uint8_t processing_type);

#endif /* QUERY_H */

