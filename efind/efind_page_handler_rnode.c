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

#include "efind_page_handler_augmented.h"
#include <string.h> //for memcpy

/*the following structures are needed for handling RNodes and REntries*/

typedef struct {
    UIPage base;
    RNode *rnode;
} UIPage_RNode;

/*free allocated memory*/
static void efind_rnode_destroy(UIPage *uipage) {
    UIPage_RNode *r = (void*) uipage;
    rnode_free(r->rnode);
    lwfree(uipage);
}

/*add a new entry in the page, cloning the entry if desired (third parameter)
  in addition, it only adds non-null entries!*/
static bool efind_rnode_add_entry(UIPage *uipage, void *entry, bool clone) {
    UIPage_RNode *r = (void*) uipage;
    REntry *rentry = (REntry*) entry;

    if (entry != NULL && rentry->bbox != NULL) {
        if (clone)
            rnode_add_rentry(r->rnode, rentry_clone(rentry));
        else
            rnode_add_rentry(r->rnode, rentry);
        return true;
    } else {
        return false;
    }
}

/*set an entry into an existing position of the page 
 * cloning the entry if desired (third parameter)
 * freeing the old entry if desired (forth parameter)
 * in addition, it only adds non-null entries!
 */
static bool efind_rnode_set_entry(UIPage *uipage, void *new_entry, int pos, bool clone, bool free_old_entry) {
    UIPage_RNode *r = (void*) uipage;
    REntry *rentry = (REntry*) new_entry;
    if (pos >= r->rnode->nofentries) {
        return false; //invalid position
    }
    if (new_entry != NULL && rentry->bbox != NULL) {
        if (free_old_entry) {
            lwfree(r->rnode->entries[pos]->bbox);
            lwfree(r->rnode->entries[pos]);
        }
        if (clone) {
            r->rnode->entries[pos] = rentry_clone(rentry);
        } else {
            r->rnode->entries[pos] = rentry;
        }
        return true;
    } else {
        return false;
    }

}

/*get the number of entries in the page*/
static int efind_rnode_get_numberofentries(const UIPage *uipage) {
    if (uipage != NULL) {
        const UIPage_RNode *r = (void*) uipage;
        return r->rnode->nofentries;
    } else {
        return 0;
    }
}

/*get a void pointer pointing to a specific entry*/
static void* efind_rnode_get_entry_at(UIPage *uipage, int position) {
    UIPage_RNode *r = (void*) uipage;
    if (r == NULL || position >= r->rnode->nofentries) {
        return NULL; //invalid position
    }
    return (void*) r->rnode->entries[position];
}

/*get the pointer of a specific entry*/
static int efind_rnode_get_pointerofentry_at(const UIPage *uipage, int position) {
    const UIPage_RNode *r = (void*) uipage;
    if (r == NULL || position >= r->rnode->nofentries) {
        return -1; //invalid position (change this value after...)
    }
    return r->rnode->entries[position]->pointer;
}

/*clone the page, returning a void pointer that corresponds to the original page type of the underlying index*/
static void* efind_rnode_clone_page(const UIPage *uipage) {
    const UIPage_RNode *r = (void*) uipage;
    return (void*) rnode_clone(r->rnode);
}

/*get the points to the page of the underlying index*/
static void* efind_rnode_get_page(const UIPage *uipage) {
    const UIPage_RNode *r = (void*) uipage;
    return (void*) r->rnode;
}

/*get the size in bytes of the current page*/
static size_t efind_rnode_get_size(const UIPage *uipage) {
    const UIPage_RNode *r = (void*) uipage;
    return rnode_size(r->rnode);
}

/*copy from source to destination (the original page type)*/
static void efind_rnode_copy_page(UIPage *dest_uipage, const UIPage *source_uipage) {
    const UIPage_RNode *source = (void*) source_uipage;
    UIPage_RNode *dest = (void*) dest_uipage;
    rnode_copy(dest->rnode, source->rnode);
}

/*get an entry (in the form of UIEntry) of an UIPage */
static UIEntry *efind_rnode_get_uientry(UIPage *uipage, int j) {
    UIPage_RNode *r = (void*) uipage;
    return efind_entryhandler_create_for_rentry(r->rnode->entries[j]);
}

/*free allocated memory*/
static void efind_rentry_destroy(UIEntry *uientry) {
    UIEntry_REntry *r = (void*) uientry;
    if (r->rentry) {
        if (r->rentry->bbox)
            lwfree(r->rentry->bbox);

        lwfree(r->rentry);
    }
    lwfree(r);
}

/*get the pointer of an entry*/
static int efind_rentry_get_pointer(const UIEntry *uientry) {
    const UIEntry_REntry *r = (void*) uientry;
    return r->rentry->pointer;
}

/*get the pointer to the entry*/
static void *efind_rentry_get(UIEntry *uientry) {
    UIEntry_REntry *r = (void*) uientry;
    return (void*) r->rentry;
}

/*get the size of an entry*/
static size_t efind_rentry_size(const UIEntry *uientry) {
    const UIEntry_REntry *r = (void*) uientry;
    if (r->rentry == NULL)
        return 0;
    if (r->rentry && r->rentry->bbox) {
        return rentry_size();
    } else {
        return sizeof (uint32_t); //the pointer    
    }
}

UIPage *efind_pagehandler_create_for_rnode(RNode *rnode) {
    UIPage_RNode *ret;

    static const UIPageInterface vtable = {efind_rnode_destroy,
        efind_rnode_add_entry, efind_rnode_set_entry, efind_rnode_get_numberofentries,
        efind_rnode_get_entry_at, efind_rnode_get_pointerofentry_at, efind_rnode_clone_page,
        efind_rnode_get_page, efind_rnode_get_size, efind_rnode_copy_page, efind_rnode_get_uientry};

    static UIPage base = {&vtable};

    ret = (UIPage_RNode*) lwalloc(sizeof (UIPage_RNode));
    memcpy(&ret->base, &base, sizeof (base));
    ret->rnode = rnode;

    return &ret->base;
}

UIPage *efind_pagehandler_create_empty_for_rnode(int nofentries) {
    UIPage_RNode *ret;

    static const UIPageInterface vtable = {efind_rnode_destroy,
        efind_rnode_add_entry, efind_rnode_set_entry, efind_rnode_get_numberofentries,
        efind_rnode_get_entry_at, efind_rnode_get_pointerofentry_at, efind_rnode_clone_page,
        efind_rnode_get_page, efind_rnode_get_size, efind_rnode_copy_page, efind_rnode_get_uientry};

    static UIPage base = {&vtable};

    ret = (UIPage_RNode*) lwalloc(sizeof (UIPage_RNode));
    memcpy(&ret->base, &base, sizeof (base));

    ret->rnode = rnode_create_empty();
    if (nofentries > 0) {
        ret->rnode->nofentries = nofentries;
        ret->rnode->entries = (REntry**) lwalloc(sizeof (REntry*) * nofentries);
    }

    return &ret->base;
}

UIPage *efind_pagehandler_create_clone_for_rnode(RNode *rnode) {
    UIPage_RNode *ret;

    static const UIPageInterface vtable = {efind_rnode_destroy,
        efind_rnode_add_entry, efind_rnode_set_entry, efind_rnode_get_numberofentries,
        efind_rnode_get_entry_at, efind_rnode_get_pointerofentry_at, efind_rnode_clone_page,
        efind_rnode_get_page, efind_rnode_get_size, efind_rnode_copy_page, efind_rnode_get_uientry};

    static UIPage base = {&vtable};

    ret = (UIPage_RNode*) lwalloc(sizeof (UIPage_RNode));
    memcpy(&ret->base, &base, sizeof (base));

    ret->rnode = rnode_clone(rnode);

    return &ret->base;
}

UIEntry *efind_entryhandler_create_for_rentry(REntry *rentry) {
    UIEntry_REntry *ret;

    static const UIEntryInterface vtable = {efind_rentry_destroy, efind_rentry_get_pointer,
        efind_rentry_get, efind_rentry_size};

    static UIEntry base = {&vtable};

    ret = (UIEntry_REntry*) lwalloc(sizeof (UIEntry_REntry));
    memcpy(&ret->base, &base, sizeof (base));
    ret->rentry = rentry;

    return &ret->base;
}
