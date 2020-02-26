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

#include <liblwgeom.h>

#include "efind_mod_handler.h"
#include "../main/festival_defs.h" //for the index types
#include "../main/math_util.h" //for DB_MIN

#include "../main/log_messages.h" //for debug
#include "efind_page_handler_augmented.h" //for operations of pages and entries

/*
 * The general key of the red-black tree is something that uniquely identifies an entry of the modified node
 * Commonly, it can be its pointer
 * For other structures that keep the entries of a node according to an order,
 *   the key of the red-black tree is a complex structure: the pointer of the entry + other info
 * For instance: 
 * (1) for the Hilbert R-tree, the key consists of the pointer + the hilbert value
 * (2) for the xBR+-tree, the key consists of the pointer + the REG of the entry (+ the REG of the entire node)
 */
static int compare_entries(const UIEntry *e1, const UIEntry *e2,
        uint8_t index_type, int node_height) {
    int i, k;

    i = efind_entryhandler_get_pofentry(e1);
    k = efind_entryhandler_get_pofentry(e2);

    //if they point to the same location, they are equal!
    if (i == k) {
        return 0;
    }

    //otherwise, the treatment is different according to the index_type

    if (index_type != eFIND_HILBERT_RTREE_TYPE) {
        //for R-tree and R*-tree, we simply keep the order of the pointers
        if (i < k)
            return -1;
        else if (i > k)
            return +1;
    } else {
        //for Hilbert R-trees, we should preserve the order of their hilbert values
        return efind_entryhandler_compare_hilbertvalues(e1, e2, node_height);
    }
    return 0; //only because of the compiler.
}

int efind_writebuffer_add_mod(eFIND_RB_Tree *tree, eFIND_Modification *mod, uint8_t index_type, int height) {
    eFIND_RB_Node **new = &(tree->rb_node), *parent = NULL;
    //we transform the new mod into an uientry
    UIEntry *uie_mod = efind_entryhandler_create(mod->entry, index_type, &height);
    UIEntry *uie_this;
    int ret = 0;

    /* Figure out where to put new node */
    while (*new) {
        eFIND_Modification *this = container_of(*new, eFIND_Modification, node);
        int result;
        //we transform the current stored entry into an uientry
        uie_this = efind_entryhandler_create(this->entry, index_type, &height);

        //the tree is formed according to a key
        result = compare_entries(uie_mod, uie_this, index_type, height);

        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else if (result > 0)
            new = &((*new)->rb_right);
        else {
            size_t old_size = efind_entryhandler_size(uie_this);
            /* In this case, we only replace the old content for the new one*/
            efind_entryhandler_destroy(uie_this);
            uie_this = NULL;

            this->entry = mod->entry;

            //if the new modification has a smaller size than the old element, than we save some space
            //otherwise, it will equal to 0
            //or finally, it will greater than 0, meaning that the new element is greater than the old element
            ret = efind_entryhandler_size(uie_mod) - old_size;

            lwfree(uie_mod);
            lwfree(mod);
            return ret;
        }
        lwfree(uie_this);
    }
    ret = efind_entryhandler_size(uie_mod) + sizeof (eFIND_Modification);
    lwfree(uie_mod);

    /* Add new node and rebalance tree. */
    rb_link_node(&mod->node, parent, new);
    rb_insert_color(&mod->node, tree);

    return ret;
}

/*this function adds an entry into a page and increment the number of elements
 it always clone the entry!
 in addition, it only adds non-null entries! Because an entry can be null, which means that this entry was removed and stored in the tree*/
static void efind_writebuffer_add_entry(UIPage *page, void *entry, int *cur_entry_pos) {
    if (*cur_entry_pos < efind_pagehandler_get_nofentries(page)) {
        //we already have allocated space - the last parameter is false since it is empty
        efind_pagehandler_set_entry(page, entry, *cur_entry_pos, true, false);
    } else {
        //we use the common add function, which uses realloc
        //TODO the clone thing should be only for the entries stored in the mod_tree, 
        //TODO then for the page the destruction should be an upper level
        efind_pagehandler_add_entry(page, entry, true);
    }

    (*cur_entry_pos)++;
}

UIPage *efind_writebuffer_merge_mods(eFIND_RB_Tree *tree, UIPage *page, uint8_t index_type, int height) {
    /*we have to form two lists of entries, both in sort order
     * the first one is all the entries stored in the tree - S1
     * the second one is the entries contained in the page - S2
     */

    //list of entries stored in the tree (IT MANTAINS THE REFERENCES FOR THE NODES OF THE TREE!)
    UIEntry **first_list = (UIEntry**) lwalloc(sizeof (UIEntry*) * 10); //S1
    UIEntry *aux = NULL;
    int max_first_list = 10;
    int size_first_list = 0;

    int size_second_list = 0;

    int cur_position_ret = 0; //the current position that we are inserting elements in the returning page

    UIPage *ret = NULL; //the returning value (which is the page merged from the modification + page)

    int i = 0, j = 0, aux_size = 0;
    
    int comp = 0;

    eFIND_RB_Node *rbnode;

    /*lets iterate our tree in order to create the first_list*/
    for (rbnode = rb_first(tree); rbnode; rbnode = rb_next(rbnode)) {
        if (size_first_list == max_first_list) {
            max_first_list *= 2;
            first_list = lwrealloc(first_list, sizeof (UIEntry*) * max_first_list);
        }

        first_list[size_first_list] = efind_entryhandler_create(
                container_of(rbnode, eFIND_Modification, node)->entry,
                index_type, &height);
        size_first_list++;
    }
    //_DEBUGF(NOTICE, "Size of the first list, which contains the modifications, is %d", size_first_list);

    /* the second list contains the elements of the page */
    size_second_list = page == NULL ? 0 : efind_pagehandler_get_nofentries(page);
    if (size_second_list == 0)
        aux_size = size_first_list;
    else
        aux_size = DB_MIN(size_first_list, size_second_list);
    //we alloc the returning page with some initial space
    ret = efind_pagehandler_create_empty(aux_size, height, index_type);

    //_DEBUGF(NOTICE, "Size of the second list, which is the node stored in the disk, is %d", size_second_list);

    /*merging operation*/
    while (i < size_first_list && j < size_second_list) {
        aux = efind_pagehandler_get_uientry_at(page, j);
        comp = compare_entries(first_list[i], aux,
                index_type, height);
        lwfree(aux); //an upper free only
        
        //if S1 is lesser than S2 than
        if (comp < 0) {
            //we put the S1 (the removed entries are correctly handled in the add_entry function)
            efind_writebuffer_add_entry(ret, efind_entryhandler_get(first_list[i]),
                    &cur_position_ret);
            i++;
        } else if (comp > 0) {
            //we put the S2            
            efind_writebuffer_add_entry(ret, efind_pagehandler_get_entry_at(page, j),
                    &cur_position_ret);
            j++;
        } else {
            //they are equal, here we only consider the most recent version of the entry (the element stored in the tree)
            //we put the S1 (the removed entries are correctly handled in the add_entry function)
            efind_writebuffer_add_entry(ret, efind_entryhandler_get(first_list[i]), &cur_position_ret);
            i++;
            j++;
        }
    }
    while (i < size_first_list) {
        //we put the S1
        efind_writebuffer_add_entry(ret, efind_entryhandler_get(first_list[i]),
                &cur_position_ret);
        i++;
    }
    while (j < size_second_list) {
        //we put the S2
        efind_writebuffer_add_entry(ret, efind_pagehandler_get_entry_at(page, j),
                &cur_position_ret);
        j++;
    }

    //only a upper free since its elements points to the original elements!
    for (i = 0; i < size_first_list; i++)
        lwfree(first_list[i]);
    lwfree(first_list);

    return ret;
}

/*recursive function to destroy the modification tree*/
static void efind_writebuffer_destroy_node_r(eFIND_RB_Node *node, uint8_t index_type, size_t *f, int height) {
    if (node) {
        UIEntry *this;
        efind_writebuffer_destroy_node_r(node->rb_left, index_type, f, height);
        efind_writebuffer_destroy_node_r(node->rb_right, index_type, f, height);

        this = efind_entryhandler_create(
                container_of(node, eFIND_Modification, node)->entry, index_type, &height);
        *f += (efind_entryhandler_size(this) + sizeof (eFIND_Modification));
        efind_entryhandler_destroy(this);
        lwfree(container_of(node, eFIND_Modification, node));
    }
}

size_t efind_writebuffer_destroy_mods(eFIND_RB_Tree *tree, uint8_t index_type, int height) {
    size_t ret = 0;
    efind_writebuffer_destroy_node_r(tree->rb_node, index_type, &ret, height);
    return ret;
}
