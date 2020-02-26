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
#include "fast_buffer_list_mod.h"

#include "../main/log_messages.h"

FASTListMod *flm_init() {
    FASTListMod *flm = (FASTListMod*) lwalloc(sizeof (FASTListMod));
    flm->first = NULL;
    flm->size = 0;
    return flm;
}

void flm_append(FASTListMod *flm, FASTModItem *item) {
    FASTListItem *fli = lwalloc(sizeof (FASTListItem));
    fli->item = item;
    fli->next = NULL;

    if (flm->first == NULL) {
        flm->first = fli;
    } else {
        FASTListItem *last = flm->first;
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = fli;
    }

    flm->size++;
}

/* free the FLM*/
void flm_destroy(FASTListMod *flm) {
    FASTListItem *current;
    while (flm->first != NULL) {
        current = flm->first;
        flm->first = current->next;

        if (current->item->type == FAST_ITEM_TYPE_K) {
            if (current->item->value.bbox != NULL) {
                lwfree(current->item->value.bbox);
            }
        }

        lwfree(current->item);
        lwfree(current);
    }
    lwfree(flm);
}

