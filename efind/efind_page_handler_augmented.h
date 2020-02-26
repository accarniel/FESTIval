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
 * File:   efind_page_handler_augmented.h
 * Author: anderson
 *
 * Created on September 17, 2017, 1:20 PM
 */

#ifndef EFIND_PAGE_HANDLER_AUGMENTED_H
#define EFIND_PAGE_HANDLER_AUGMENTED_H

#include "efind_page_handler.h"
#include "../rtree/rnode.h"
#include "../hilbertrtree/hilbert_node.h"

/* a common struct for Hilbert R-tree, R*-tree, and R-tree indices*/
typedef struct {
    UIEntry base;
    REntry *rentry;
} UIEntry_REntry;

/* for RTree and RStarTree (implemented in efind_page_handler_rnode.c) */
extern UIPage *efind_pagehandler_create_for_rnode(RNode *rnode);
extern UIPage *efind_pagehandler_create_clone_for_rnode(RNode *rnode);
extern UIPage *efind_pagehandler_create_empty_for_rnode(int nofentries);
extern UIEntry *efind_entryhandler_create_for_rentry(REntry *rentry);

/* for HilbertRTree (implemented in efind_page_handler_hilbertnode.c) */
extern UIPage *efind_pagehandler_create_for_hilbertnode(HilbertRNode *node);
extern UIPage *efind_pagehandler_create_clone_for_hilbertnode(HilbertRNode *rnode);
extern UIPage *efind_pagehandler_create_empty_for_hilbertnode(int nofentries, int height);
extern UIEntry *efind_entryhandler_create_for_hilbertentry(HilbertIEntry *entry);
extern void efind_pagehandler_set_srid(int srid);
extern int efind_entryhandler_compare_hilbertvalues(const UIEntry *e1, const UIEntry *e2, int height);

#endif /* EFIND_PAGE_HANDLER_AUGMENTED_H */

