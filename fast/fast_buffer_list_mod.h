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
 * File:   fast_buffer_list_mod.h
 * Author: Anderson Chaves Carniel
 *
 * Created on March 30, 2016, 4:42 PM
 */

#ifndef FAST_BUFFER_LIST_MOD_H
#define FAST_BUFFER_LIST_MOD_H

#include "../main/bbox_handler.h" //for BBOX
#include "../hilbertrtree/hilbert_node.h" //for hilbert value

/*types of the values in the modification list*/
#define FAST_ITEM_TYPE_K    1
#define FAST_ITEM_TYPE_P    2
#define FAST_ITEM_TYPE_L    3
#define FAST_ITEM_TYPE_H    4 //for holes in hilbert nodes

/*this is the struct of a item of the modification for the list of modification 
 * in the FAST buffer*/
typedef struct {
    uint8_t type; //can be K, L or P (we group Pf and Pm in order to handle it better in the implementation) 
    //the value can be a BBOX (when type equal to k) or int (when type equal to P)
    //or hilbert_value_t (when type equal to L)
    int position; //the index of the modified entry in the rnode
    union {
        int pointer; //it is only a valid value if type is equal to P
        BBox *bbox; //it is only a valid value if type is equal to K
        hilbert_value_t lhv; //it is only a valid value if type is equal to L
    } value;
} FASTModItem;

/*node in the list of modification*/
typedef struct _fast_list_item {
    struct _fast_list_item *next;
    FASTModItem *item;
} FASTListItem;

/*linked list of the modifications*/
typedef struct {
    int size;
    FASTListItem *first;
} FASTListMod;

/*create a new list of modifications of a hash entry*/
extern FASTListMod *flm_init(void);

/*append a new modification (FLM_ITEM)*/
extern void flm_append(FASTListMod *flm, FASTModItem *item);

/* free the FLM*/
extern void flm_destroy(FASTListMod *flm);

#endif /* _FAST_BUFFER_LIST_MOD_H */

