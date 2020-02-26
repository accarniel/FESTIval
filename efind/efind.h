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
 * File:   efind.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 14, 2017, 1:08 AM
 * Modified on April 2, 2017
 */

#ifndef EFIND_H
#define EFIND_H

#include "../efind/efind_spec.h" //for EFINDSpec
#include "../rtree/rtree.h" //for Rtree
#include "../rstartree/rstartree.h" //for RStartree
#include "../hilbertrtree/hilbertrtree.h" //for HilbertRtree

/*here we define all the possible variations (by using our indices) for the eFIND
 This framework is originally proposed by Anderson Chaves Carniel
 * first publication:
 * Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. A Generic and Efficient Framework for
Spatial Indexing on Flash-based Solid State Drives. In 21st European Conference on
Advances in Databases and Information Systems (ADBIS), p. 229-243, 2017
 * The eFIND implementation contained at FESTIval v2.3 is a great extension from this published work, published in:

Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. A generic and efficient framework for flash-aware spatial indexing. Information Systems 82, p. 102-120, 2019. 
 */

/* Types of flushing policies */
#define eFIND_M_FP                     10 //it flushes nodes chosen by using the number of modifications only       
#define eFIND_MT_FP                    11 //it flushes nodes chosen by using the number of mod. and the timestamp
#define eFIND_MTH_FP                   12 //it flushes nodes chosen by using the number of mod., timestamp, and height
#define eFIND_MTHA_FP                  13 //it flushes nodes chosen by using the number of mod., timestamp, height, and coverage area
#define eFIND_MTHAO_FP                 14 //it flushes using the number of mod., timestamp, height, coverage area, and overlapping area

/* types of temporal control policies */
#define eFIND_NONE_TCP                 0 //without a temporal control policy
#define eFIND_READ_TCP                 20 //it forces the flushing nodes to be stored in the read buffer if they are frequently read
#define eFIND_WRITE_TCP                21 //it returns the nodes that are very near to or far away (stride) to the most recent written nodes
#define eFIND_READ_WRITE_TCP           22 //combine both policies

/* Types of policies for the page replacement employed by the read buffer */
#define eFIND_NONE_RBP                 0 //there is no read buffer
#define eFIND_LRU_RBP                  1 //it is based on the LRU       
#define eFIND_HLRU_RBP                 2 //it is based on the HLRU, which prioritizes nodes in the highest levels of the tree
#define eFIND_S2Q_RBP                  3 //it is based on the Simplified version of the 2Q, the size of the A1 is defined as the read_temporal_control_perc value!
#define eFIND_2Q_RBP                   4 //it is based on the full version of the 2Q, the parameters of this buffer is defined by eFIND2QSpecification structure

/*the additional parameter of the S2Q is the size of the Ain, indicated as a percentage value*/
typedef struct {
    //the size of A1out is the size of read_temporal_control_perc
    //the size of Am is the remaining size of the read buffer
    double A1in_perc_size;
} eFIND2QSpecification;

/* THE DEFINITION OF An eFIND R-TREE INDEX */
typedef struct {   
    RTree *rtree;
    eFINDSpecification *spec;    
} eFINDRTree;

/* THE DEFINITION OF An eFIND R*-TREE INDEX */
typedef struct {
    RStarTree *rstartree;
    eFINDSpecification *spec;    
} eFINDRStarTree;

/* THE DEFINITION OF An eFIND Hilbert R-TREE INDEX */
typedef struct {
    HilbertRTree *hilbertrtree;
    eFINDSpecification *spec;    
} eFINDHilbertRTree;

/* THE DEFINITION OF A GENERIC eFIND INDEX as a subtype of spatialindex */
typedef struct {
    /*this includes the source, generic parameters, and general functions
     * it is important to note that RStarTree and RTree have also this attribute, 
     * which will ALWAYS point to the same reference!
     */
    SpatialIndex base;
    //internal control (e.g., statistical data and to write data blocks)
    uint8_t efind_type_index; //e.g. FAST_RTREE and so on

    union {
        eFINDRTree *efind_rtree;
        eFINDRStarTree *efind_rstartree;
        eFINDHilbertRTree *efind_hilbertrtree;
    } efind_index;
} eFINDIndex;

/* specific parameters of the R-tree should be specified later (therefore, the caller is responsible for it)*/
extern SpatialIndex *efindrtree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, eFINDSpecification *fs, bool persist);

/* specific parameters of the R*-tree should be specified later (therefore, the caller is responsible for it)*/
extern SpatialIndex *efindrstartree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, eFINDSpecification *fs, bool persist);

/* specific parameters of the Hilbert R-tree should be specified later (therefore, the caller is responsible for it)*/
extern SpatialIndex *efindhilbertrtree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, eFINDSpecification *fs, bool persist);

#endif /* EFIND_H */

