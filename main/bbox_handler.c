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

#include "bbox_handler.h"
#include "math_util.h"

#include <string.h>

BBox *bbox_create() {
    BBox *bbox = (BBox*) lwalloc(sizeof (BBox));    
    return bbox;
}

void gbox_to_bbox(const GBOX *gbox, BBox *bbox) {
    if (NUM_OF_DIM == 2) {
        bbox->min[0] = gbox->xmin;
        bbox->min[1] = gbox->ymin;

        bbox->max[0] = gbox->xmax;
        bbox->max[1] = gbox->ymax;
    }
}

LWGEOM *bbox_to_geom(const BBox *box) {
    if (NUM_OF_DIM == 2) {
        /*
         * In order to always return a valid geometry:
         *     - If the bounding box is a single point then return a
         *     POINT geometry
         *     - If the bounding box represents either a horizontal or
         *     vertical line, return a LINESTRING geometry
         *     - Otherwise return a POLYGON
         */

        if ((box->min[0] == box->max[0]) && (box->min[1] == box->max[1])) {
            /* Construct a point */
            LWPOINT *point = lwpoint_make2d(SRID_UNKNOWN, box->min[0], box->min[1]);
            return lwpoint_as_lwgeom(point);
        } else if ((box->min[0] == box->max[0]) || (box->min[1] == box->max[1])) {
            LWLINE *line;
            POINTARRAY *pa;
            POINT4D pt;
            
            /* Construct point array */
            pa = ptarray_construct_empty(0, 0, 2);

            /* Assign coordinates to POINT2D array */
            pt.x = box->min[0]; //xmin;
            pt.y = box->min[1]; //ymin;
            ptarray_append_point(pa, &pt, LW_TRUE);
            pt.x = box->max[0]; //xmax;
            pt.y = box->max[1]; //ymax;
            ptarray_append_point(pa, &pt, LW_TRUE);

            /* Construct and serialize linestring */
            line = lwline_construct(SRID_UNKNOWN, NULL, pa);
            return lwline_as_lwgeom(line);
        } else {
            LWPOLY *poly;
            POINTARRAY *pa = ptarray_construct_empty(0, 0, 5);
            POINT4D pt;
            POINTARRAY **ppa = lwalloc(sizeof (POINTARRAY*));

            /* Assign coordinates to point array */
            pt.x = box->min[0]; //xmin
            pt.y = box->min[1]; //ymin
            ptarray_append_point(pa, &pt, LW_TRUE);
            pt.x = box->min[0]; //xmin
            pt.y = box->max[1]; //ymax
            ptarray_append_point(pa, &pt, LW_TRUE);
            pt.x = box->max[0]; //xmax
            pt.y = box->max[1]; //ymax
            ptarray_append_point(pa, &pt, LW_TRUE);
            pt.x = box->max[0]; //xmax
            pt.y = box->min[1]; //ymin
            ptarray_append_point(pa, &pt, LW_TRUE);
            pt.x = box->min[0]; //xmin
            pt.y = box->min[1]; //ymin
            ptarray_append_point(pa, &pt, LW_TRUE);

            /* Construct polygon */
            ppa[0] = pa;
            poly = lwpoly_construct(SRID_UNKNOWN, NULL, 1, ppa);
            return lwpoly_as_lwgeom(poly);
        }
    } else {
        return NULL;
    }
}

/* filtering predicates for BBOXes
 * if both objects are pure rectangles, that is, they are not only approximations
 * then we are able to use these functions to check their topological relationships
 * 
 * otherwise, we use the following:
 * intersect to verify if the objects are disjoint, meeting, and overlapping
 * inside_or_coveredBy to verify if there is an containment relationship, like: inside, contains, coveredBy, and covers
 * equal to verify if the object are equals
 *  */
static bool intersect(const BBox *bbox1, const BBox *bbox2); //its complement defines the disjoint
static bool overlap(const BBox *bbox1, const BBox *bbox2);
static bool meet(const BBox *bbox1, const BBox *bbox2);
static bool coveredBy(const BBox *bbox1, const BBox *bbox2);
static bool inside(const BBox *bbox1, const BBox *bbox2);
static bool inside_or_coveredBy(const BBox *bbox1, const BBox *bbox2);
static bool equal(const BBox *bbox1, const BBox *bbox2);

bool intersect(const BBox *bbox1, const BBox *bbox2) {
    int d = -1;
    do {
        d++;
        /* if they do not share any point, then return false */
        if (DB_LT(bbox1->max[d], bbox2->min[d]) ||
                DB_GT(bbox1->min[d], bbox2->max[d]))
            return false;
    } while (d != MAX_DIM);
    return true;
}

/* this is inside OR coveredBy predicate - to check if there is a containment relationship*/
bool inside_or_coveredBy(const BBox *bbox1, const BBox *bbox2) {
    int d = -1;
    bool o;
    do {
        d++;
        /*check for each dimension, if bbox1 is inside bbox2
         */
        o = (DB_GE(bbox1->min[d], bbox2->min[d]) &&
                DB_LE(bbox1->max[d], bbox2->max[d]));
    } while (o && d != MAX_DIM);
    return o;
}

/*is bbox1 inside bbox2?*/
bool inside(const BBox *bbox1, const BBox *bbox2) {
    int d = -1;
    bool o;
    do {
        d++;
        /*check for each dimension, if bbox1 is inside bbox2
         */
        o = (DB_GT(bbox1->min[d], bbox2->min[d]) &&
                DB_LT(bbox1->max[d], bbox2->max[d]));
    } while (o && d != MAX_DIM);
    return o;
}

/*is bbox1 covered by bbox2?*/
bool coveredBy(const BBox *bbox1, const BBox *bbox2) {
    /*
     * check if bbox1 is inside bbox2 and check if they share some edge
     */
    int d = -1;
    bool o;
    do {
        d++;
        /*check for each dimension, if they possibly share any edge 
         * and if there is a containment
         */
        o = ((DB_GE(bbox1->min[d], bbox2->min[d]) &&
                DB_LE(bbox1->max[d], bbox2->max[d])) &&
                (DB_IS_EQUAL(bbox1->min[d], bbox2->min[d]) ||
                DB_IS_EQUAL(bbox1->max[d], bbox2->max[d])));
    } while (o && d != MAX_DIM);
    return o;
}

/* this overlap is according to the 9-IM! */
bool overlap(const BBox *bbox1, const BBox *bbox2) {
    int d = -1;
    bool o;
    do {
        d++;
        /*check for each dimension, if bbox1 overlap bbox2
         * it does not consider border intersections!!
         */
        o = (DB_LT(bbox1->min[d], bbox2->max[d]) &&
                DB_GT(bbox2->max[d], bbox1->min[d]));
    } while (o && d != MAX_DIM);
    //check if there are not containment relationships, since this is the overlap predicate
    return o
            && !inside(bbox1, bbox2)
            && !inside(bbox2, bbox1)
            && !coveredBy(bbox1, bbox2)
            && !coveredBy(bbox2, bbox1);
}

/* this meet is according to the 9-IM! */
bool meet(const BBox *bbox1, const BBox *bbox2) {
    /* if they are not disjoint (i.e., intersect), 
     * and they do not overlap in multidimensional area
     then they are adjacent*/
    return intersect(bbox1, bbox2) && !overlap(bbox1, bbox2);
}

bool equal(const BBox *bbox1, const BBox *bbox2) {
    int d = -1;
    do {
        d++;
        /* check if they are equal, for each dimension */
        if (DB_IS_NOT_EQUAL(bbox1->max[d], bbox2->max[d]) ||
                DB_IS_NOT_EQUAL(bbox1->min[d], bbox2->min[d])) {
            return false;
        }
    } while (d != MAX_DIM);
    return true;
}

bool bbox_check_predicate(const BBox *bbox1, const BBox *bbox2, uint8_t predicate) {
    switch (predicate) {
        case INTERSECTS:
            return intersect(bbox1, bbox2);
        case DISJOINT:
            return !intersect(bbox1, bbox2);
        case OVERLAP:
            return overlap(bbox1, bbox2);
        case MEET:
            return meet(bbox1, bbox2);
        case INSIDE:
            return inside(bbox1, bbox2);
        case CONTAINS:
            return inside(bbox2, bbox1);
        case COVEREDBY:
            return coveredBy(bbox1, bbox2);
        case COVERS:
            return coveredBy(bbox2, bbox1);
        case EQUAL:
            return equal(bbox1, bbox2);
        case INSIDE_OR_COVEREDBY:
            return inside_or_coveredBy(bbox1, bbox2);
        case CONTAINS_OR_COVERS:
            return inside_or_coveredBy(bbox2, bbox1);
        default:
            return false;
    }
    return false;
}

double bbox_area(const BBox *bbox) {
    int i;
    double area = 1.0;
    for (i = 0; i <= MAX_DIM; i++)
        area *= (bbox->max[i] - bbox->min[i]);
    return area;
}

BBox *bbox_union(const BBox *bbox1, const BBox *bbox2) {
    BBox *un = bbox_create();
    int i;

    for (i = 0; i <= MAX_DIM; i++) {
        un->max[i] = DB_MAX(bbox1->max[i], bbox2->max[i]);
        un->min[i] = DB_MIN(bbox1->min[i], bbox2->min[i]);
    }
    return un;
}

void bbox_increment_union(const BBox *input, BBox *un) {
    int i;

    for (i = 0; i <= MAX_DIM; i++) {
        un->max[i] = DB_MAX(input->max[i], un->max[i]);
        un->min[i] = DB_MIN(input->min[i], un->min[i]);
    }
}

void bbox_expanded_area_and_union(const BBox *input, const BBox *bbox_node, BBox *un, double *area) {
    int i;
    double un_area = 1.0;
    double bbox_node_area = 1.0;

    for (i = 0; i <= MAX_DIM; i++) {
        un->max[i] = DB_MAX(input->max[i], bbox_node->max[i]);
        un->min[i] = DB_MIN(input->min[i], bbox_node->min[i]);

        un_area *= (un->max[i] - un->min[i]);
        bbox_node_area *= (bbox_node->max[i] - bbox_node->min[i]);
    }
    *area = un_area - bbox_node_area;
}

double bbox_area_of_union(const BBox *bbox1, const BBox *bbox2) {
    double max, min;
    int i;
    double area = 1.0;

    for (i = 0; i <= MAX_DIM; i++) {
        max = DB_MAX(bbox1->max[i], bbox2->max[i]);
        min = DB_MIN(bbox1->min[i], bbox2->min[i]);

        area *= (max - min);
    }
    return area;
}

double bbox_area_of_required_expansion(const BBox *input, const BBox *bbox_node) {
    double max, min;
    int i;
    double union_area = 1.0;
    double bbox_node_area = 1.0;

    for (i = 0; i <= MAX_DIM; i++) {
        max = DB_MAX(input->max[i], bbox_node->max[i]);
        min = DB_MIN(input->min[i], bbox_node->min[i]);

        union_area *= (max - min);
        bbox_node_area *= (bbox_node->max[i] - bbox_node->min[i]);
    }
    return union_area - bbox_node_area;
}

double bbox_overlap_area(const BBox *bbox1, const BBox *bbox2) {
    double max, min;
    int i;
    double area = 1.0;

    for (i = 0; i <= MAX_DIM; i++) {
        if (bbox1->min[i] < bbox2->min[i])
            min = bbox2->min[i];
        else
            min = bbox1->min[i];

        if (bbox1->max[i] < bbox2->max[i])
            max = bbox1->max[i];
        else
            max = bbox2->max[i];

        area *= (max - min);
    }
    return area;
}

BBoxCenter *bbox_get_center(const BBox *bbox) {
    int i;
    BBoxCenter *c;
    c = (BBoxCenter*) lwalloc(sizeof (BBoxCenter));
    for (i = 0; i <= MAX_DIM; i++) {
        c->center[i] = (bbox->min[i] + bbox->max[i]) / 2.0;
    }
    return c;
}

double bbox_distance_between_centers(const BBoxCenter *c1, const BBoxCenter *c2) {
    double sum = 0.0;
    int i;
    for (i = 0; i <= MAX_DIM; i++) {
        sum += (c1->center[i] - c2->center[i]) * (c1->center[i] - c2->center[i]);
    }
    return sum; //without sqrt - relative distance
}

BBox *bbox_clone(const BBox *bbox) {
    BBox *ret = (BBox*) lwalloc(sizeof (BBox));
    memcpy(ret, bbox, sizeof (BBox));
    return ret;
}
