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
 * File:   efind_mod_handler.h
 * Author: anderson
 *
 * Created on September 14, 2017, 3:02 PM
 */

#ifndef EFIND_MOD_HANDLER_H
#define EFIND_MOD_HANDLER_H

#include "../libraries/rbtree-linux/rbtree.h"
#include <stdint.h>
#include "efind_page_handler.h" //for handling the entries/nodes

/*this file provides the needed interface for managing the modification of nodes/pages for eFIND
 eFIND employs a red-black tree to store the modifications of a node/page in the write buffer
 we use the red-black tree implementation of linux - see the folder libraries/rbtree-linux for details*/

typedef struct rb_root eFIND_RB_Tree;
typedef struct rb_node eFIND_RB_Node;

typedef struct {
    eFIND_RB_Node node;
    void *entry; //this is the entire entry of a node/page of the underlying index
    //the key of the red-black tree will be the RRN that the entry points to
} eFIND_Modification;

/*this function adds a new modification or change its content if the modification is already stored
 entry here is a void pointer that can be any entry handler by the underlying index
 e.g., a HREntry or REntry - according to the index_type
 it returns the size that was added in the red-black tree (it can even return 0, if a replacement was did) */
extern int efind_writebuffer_add_mod(eFIND_RB_Tree *tree, eFIND_Modification *mod,
        uint8_t index_type, int height);

/*this function employs a merge algorithm to combine the modifications stored in the rb-tree
 and the node/page stored in the storage device
 therefore, it returns the most recent version of a node/page
 where its entries are sorted
 the caller is responsible to free "page"*/
extern UIPage *efind_writebuffer_merge_mods(eFIND_RB_Tree *tree, UIPage *page,
        uint8_t index_type, int height);

/*destroy a rb-tree and return its freed size*/
extern size_t efind_writebuffer_destroy_mods(eFIND_RB_Tree *tree,
        uint8_t index_type, int height);

#endif /* EFIND_MOD_HANDLER_H */

