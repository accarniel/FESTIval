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
 * File:   festival_defs.h
 * Author: Anderson Chaves Carniel
 *
 * Created on September 24, 2016, 6:48 PM
 */

/**** This .h specifies general definitions used in FESTIval ***/

#ifndef FESTIVAL_DEFS_H
#define FESTIVAL_DEFS_H

#include <stdint.h> /* for uint8_t*/
#include <stdbool.h> /*for bool */

/***********************************
 macros to define the type of index (for internal control)
 * the values here should be sequential because we use JUMP TABLEs in some files
 ***********************************/
#define CONVENTIONAL_RTREE              1 //original r-tree defined by guttman
#define CONVENTIONAL_RSTARTREE          2 //original R*-Tree 
#define CONVENTIONAL_HILBERT_RTREE      3 //original hilbert r-tree

#define FAST_RTREE_TYPE                 4 //r-tree for flash systems (fast r-tree)
#define FAST_RSTARTREE_TYPE             5 //r*-tree for flash systems (fast r*-tree)
#define FAST_HILBERT_RTREE_TYPE         6 //hilbert r-tree for flash systems (fast hilbert r-tree)

#define FORTREE_TYPE                    7 //for-tree for flash systems

#define eFIND_RTREE_TYPE                8 //r-tree for flash systems (efind r-tree)
#define eFIND_RSTARTREE_TYPE            9 //r*-tree for flash systems (efind r*-tree)
#define eFIND_HILBERT_RTREE_TYPE        10 //hilbert r-tree for flash systems (efind hilbert r-tree)

/************************************
 macros to define the split type for R-tree indices
 ************************************/

#define NONE_SPLIT                      0 //No split technique
#define RTREE_LINEAR_SPLIT              1 //R-TREE with linear-cost split
#define RTREE_QUADRATIC_SPLIT           2 //R-TREE with quadratic-cost split
#define RTREE_EXPONENTIAL_SPLIT         3 //R-TREE with exponential-cost split (exauhistive)
#define RSTARTREE_SPLIT                 4 //R*-TREE default split algorithm
#define GREENE_SPLIT                    5 //Greene's split
#define ANGTAN_SPLIT                    6 //AngTan's split

/*****************************************
 * Types of reinsert for R*-tree
 ******************************************/

#define CLOSE_REINSERT                  1
#define FAR_REINSERT                    2

/**********************************************************************************
 * SPECIFICATION OF A BASIC STRUCTURE TO DEFINE R-TREE-BASED INDICES (e.g., R*-TREE)
 ***********************************************************************************/

/*this struct is responsible to store the common informations from spatial indices based on the R-tree
 this means that all R-tree indices (like R*-tree, Hilbert R-tree) will use this struct */
typedef struct {
    int root_page; //which is the page number for the root node?
    int height; //the height of the tree
    int last_allocated_page; //the number of allocated page (the last page allocated is..)

    int* empty_pages; //an array of empty pages that are able to be used in the future
    int max_empty_pages; //the total capacity of empty_pages
    int nof_empty_pages; //the number of pages of empty_pages
} RTreesInfo;

/*create a common_rtrees_info object WITHOUT empty_pages!*/
extern RTreesInfo *rtreesinfo_create(int rp, int h, int lap);
extern void rtreesinfo_free(RTreesInfo *cri);
extern void rtreesinfo_set_empty_pages(RTreesInfo *cri,
        int *empty_pages, int nof, int max);
/*functions to add/remove an empty page (i.e., a page that was previously freed)*/
extern void rtreesinfo_add_empty_page(RTreesInfo *cri, int page);
extern void rtreesinfo_remove_empty_page(RTreesInfo *cri, int position);
/* function to return a new free page number
 * it firstly check if there is an empty page (i.e., a page that was previously freed)
 * in negative case, we alloc a new page */
extern int rtreesinfo_get_valid_page(RTreesInfo *cri);


/* functions to extract occupancy information, when applicable */
extern int rtreesinfo_get_max_entries(uint8_t idx_type, int page_size, int entry_size, double ratio);
extern int rtreesinfo_get_min_entries(uint8_t idx_type, int max_entries, double ratio);

/*a generic function to check if an array contains an integer element*/
extern bool array_contains_element(int *vec, int n, int v);

#endif /* _FESTIVAL_DEFS_H */

