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
 * File:   bbox_handler.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 25, 2016, 7:57 PM
 */

#ifndef BBOX_HANDLER_H
#define BBOX_HANDLER_H

#include <liblwgeom.h>
#include <stdbool.h>
#include "spatial_approximation.h"

/* definition of a multidimensional bounding box 
 * min and max are arrays
 *      each position i of min and max indicates 
 *      a minimum and maximum coordinate for dimension i
 * for instance, position [0] means the coordinate x, position[1] means the coordinate y
 */
typedef struct {
    double min[NUM_OF_DIM];
    double max[NUM_OF_DIM];
} BBox;

/*center of a bbox*/
typedef struct {
    double center[NUM_OF_DIM];
} BBoxCenter;

/*****************
 BASIC FUNCTIONS
 */

/*new instance of bbox, the caller is responsible to free it*/
extern BBox *bbox_create(void);

/*convert postgis gbox into our bbox*/
extern void gbox_to_bbox(const GBOX *gbox, BBox *bbox);

/*convert bbox to a postgis object (that is, a polygon in the rectangle format) */
extern LWGEOM *bbox_to_geom(const BBox *bbox);

/******
 * computation of topological relationships between BBOXes, that returns TRUE OR FALSE
 */
extern bool bbox_check_predicate(const BBox *bbox1, const BBox *bbox2, uint8_t predicate);

/**
 * computation of the area (this is useful for ties in the ChooseLeaf algorithm
 * In fact, this corresponds to the volume of a multidimensional object!
 */
extern double bbox_area(const BBox *bbox);

/*
 * computation of the enlargement bbox to include a bbox into another (R-tree)
 */
extern BBox *bbox_union(const BBox *bbox1, const BBox *bbox2);

/*incremental union between two bboxes*/
extern void bbox_increment_union(const BBox *input, BBox *un);

/*
 * it computes the union area between two bboxes
 */
extern double bbox_area_of_union(const BBox *bbox1, const BBox *bbox2);

/*
 * it computed the required expansion area to include the bbox input into the bbox_node
 */
extern double bbox_area_of_required_expansion(const BBox *input, const BBox *bbox_node);

/*
* we compute the union between two bboxes and store it in un
 * we also compute the expanded area of un in order to include input into the bbox of a node (stored in area)
 */
extern void bbox_expanded_area_and_union(const BBox *input, const BBox *bbox_node, BBox *un, double *area);

/*computation of the area of the overlapping area between two bboxes*/
extern double bbox_overlap_area(const BBox *bbox1, const BBox *bbox2);

/*it calculates the center of a bbox*/
extern BBoxCenter *bbox_get_center(const BBox *bbox);

/*it calculates the distance between two centers for the R*-tree*/
double bbox_distance_between_centers(const BBoxCenter *c1, const BBoxCenter *c2);

extern BBox *bbox_clone(const BBox *bbox);

#endif /* _BBOX_HANDLER_H */

