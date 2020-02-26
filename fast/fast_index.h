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
 * File:   fast_index.h
 * Author: Anderson Chaves Carniel
 *
 * Created on March 31, 2016, 8:20 PM
 */

#ifndef FAST_INDEX_H
#define FAST_INDEX_H

#include "../fast/fast_spec.h" //for FASTSpec
#include "../rtree/rtree.h" //for Rtree
#include "../rstartree/rstartree.h" //for RStartree
#include "../hilbertrtree/hilbertrtree.h" //for HilbertRtree

/*here we define all the possible variations (by using our indices) for the FAST
 according to
 * Reference: SARWAT, M.; MOKBEL, M. F.; ZHOU, X.; NATH, S. Generic and efficient framework for search trees on flash memory storage systems. Ge-
oInformatica, Springer US, v. 17, n. 3, p. 417–448, 2013.
 */


/* Types of flushing policies */
#define FLUSH_ALL                       1 //it flushes all the modifications from the main memory to flash memory
#define RANDOM_FLUSH                    2 //it flushes a random node with N modifications
#define FAST_FLUSHING_POLICY            3 //it flushes a node choose by the fast flushing policy
#define FAST_STAR_FLUSHING_POLICY       4 //it flushes a node choose by the fast* flushing policy

/* THE DEFINITION OF A FAST R-TREE INDEX */
typedef struct {   
    RTree *rtree;
    FASTSpecification *spec;    
} FASTRTree;

/* THE DEFINITION OF A FAST R*-TREE INDEX */
typedef struct {
    RStarTree *rstartree;
    FASTSpecification *spec;    
} FASTRStarTree;

/* THE DEFINITION OF A FAST HILBERT R-TREE INDEX */
typedef struct {
    HilbertRTree *hilbertrtree;
    FASTSpecification *spec;    
} FASTHilbertRTree;

/* THE DEFINITION OF A GENERIC FAST INDEX as a subtype of spatialindex */
typedef struct {
    /*this includes the source, generic parameters, and general functions
     * it is important to note that RStarTree and RTree have also this attribute, 
     * which will ALWAYS point to the same reference!
     */
    SpatialIndex base;
    //internal control (e.g., statistical data and to write data blocks)
    uint8_t fast_type_index; //e.g. FAST_RTREE and so on

    union {
        FASTRTree *fast_rtree;
        FASTRStarTree *fast_rstartree;
        FASTHilbertRTree *fast_hilbertrtree;
    } fast_index;
} FASTIndex;

/* specific parameters of the R-tree should be specified later (therefore, the caller is responsible for it)*/
extern SpatialIndex *fastrtree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, FASTSpecification *fs, bool persist);

/* specific parameters of the R*-tree should be specified later (therefore, the caller is responsible for it)*/
extern SpatialIndex *fastrstartree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, FASTSpecification *fs, bool persist);

/* specific parameters of the Hilbert R-tree should be specified later (therefore, the caller is responsible for it)*/
extern SpatialIndex *fasthilbertrtree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, FASTSpecification *fs, bool persist);


#endif /* FAST_INDEX_H */

