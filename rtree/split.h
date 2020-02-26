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
 * File:   split.h
 * Author: Anderson Chaves Carniel
 *
 * Created on March 7, 2016, 12:04 AM
 */

#ifndef SPLIT_H
#define SPLIT_H

#include "../rstartree/rstartree.h" /* for the RStarTreeSpecification and RTreeSpecification */

/*
 * This is the function that implements the all split algorithms specified in the R-tree original paper:
 * Reference: GUTTMAN, A. R-trees: A dynamic index structure for spatial searching. SIGMOD Record,
ACM, New York, NY, USA, v. 14, n. 2, p. 47–57, 1984.
 * 
 * their names are defined according to its complexities
 */
/* this function do not write nodes! The caller is responsible to do it. */
extern void split_node(const RTreeSpecification *rs, RNode *input, int input_level, RNode *l, RNode *ll);

/* this is the function that implements the split algorithm of the R*-tree
 Reference: BECKMANN, N.; KRIEGEL, H.-P.; SCHNEIDER, R.; SEEGER, B. The R*-tree: An efficient
and robust access method for points and rectangles. SIGMOD Record, ACM, v. 19, n. 2, p.
322–331, 1990.*/
extern void rstartree_split_node(const RStarTreeSpecification *rs, RNode *input, int input_level, RNode *l, RNode *ll);

#endif /* _SPLIT_H */

