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

#include "efind_page_handler.h"
#include "efind_page_handler_augmented.h"
#include "../main/festival_defs.h"
#include "../main/log_messages.h"

/*functions to delegate which type of UIPage and UIEntry the eFIND must create*/

/************************************
 Functions to Create a UIPage (according to the supported efind indices)
 ************************************/

/*this function creates an UIPage from a page/node of a supported underlying index*/
UIPage *efind_pagehandler_create(void *p, uint8_t index_type) {
    if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
        return efind_pagehandler_create_for_rnode((RNode*) p);
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
        return efind_pagehandler_create_for_hilbertnode((HilbertRNode*) p);
    } else {
        _DEBUGF(ERROR, "Index %d not supported by eFIND", index_type);
    }
}

/*this function creates an UIPage with some initial allocated space (ONLY SPACE, THE ENTRIES ARE NULL)*/
UIPage *efind_pagehandler_create_empty(int nofentries, int height, uint8_t index_type) {
    if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
        return efind_pagehandler_create_empty_for_rnode(nofentries);
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
        return efind_pagehandler_create_empty_for_hilbertnode(nofentries, height);
    } else {
        _DEBUGF(ERROR, "Index %d not supported by eFIND", index_type);
    }
}

UIPage *efind_pagehandler_create_clone(void *p, uint8_t index_type) {
    if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
        return efind_pagehandler_create_clone_for_rnode((RNode*) p);
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
        return efind_pagehandler_create_clone_for_hilbertnode((HilbertRNode*) p);
    } else {
        _DEBUGF(ERROR, "Index %d not supported by eFIND", index_type);
    }
}

/************************************
 Functions to Create an UIEntry (according to the supported efind indices)
 ************************************/

UIEntry *efind_entryhandler_create(void *e, uint8_t index_type, const void *param) {
    if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
        return efind_entryhandler_create_for_rentry((REntry*) e);
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
        int height = *((int*) param);
        if (height > 0)
            return efind_entryhandler_create_for_hilbertentry((HilbertIEntry*) e);
        else
            return efind_entryhandler_create_for_rentry((REntry*) e);
    } else {
        _DEBUGF(ERROR, "Index %d not supported by eFIND", index_type);
    }
}
