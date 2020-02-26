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

static int _global_srid = 0;

void efind_pagehandler_set_srid(int srid) {
    _global_srid = srid;
}

/*the following structures are needed for handling HilbertRNode and HilbertIEntry*/

typedef struct {
    UIPage base;
    HilbertRNode *hilbertnode;
} UIPage_HilbertRNode;

typedef struct {
    UIEntry base;
    HilbertIEntry *hentry;
} UIEntry_HilbertIEntry;


/*the needed comparator for efind_mod_handler*/
int efind_entryhandler_compare_hilbertvalues(const UIEntry *e1, const UIEntry *e2,
        int height) {
    hilbert_value_t h1, h2;
    int p1, p2;
    if (height == 0) {
        //leaf node
        const UIEntry_REntry *re1 = (void*) e1;
        const UIEntry_REntry *re2 = (void*) e2;
        
        h1 = hilbertvalue_compute(re1->rentry->bbox, _global_srid);
        h2 = hilbertvalue_compute(re2->rentry->bbox, _global_srid);

        p1 = re1->rentry->pointer;
        p2 = re2->rentry->pointer;
    } else {
        //internal node
        const UIEntry_HilbertIEntry *re1 = (void*) e1;
        const UIEntry_HilbertIEntry *re2 = (void*) e2;

        h1 = re1->hentry->lhv;
        h2 = re2->hentry->lhv;

        p1 = re1->hentry->pointer;
        p2 = re2->hentry->pointer;
    }

    if (h1 < h2) {
        return -1;
    } else if (h1 > h2) {
        return +1;
    } else {
        //they have equal hilbert values, then we sort by the pointer
        if (p1 < p2)
            return -1;
        else if (p1 > p2)
            return +1;
    }
    return 0;
}

/*free allocated memory*/
static void efind_hilbertnode_destroy(UIPage *uipage) {
    UIPage_HilbertRNode *r = (void*) uipage;
    hilbertnode_free(r->hilbertnode);
    lwfree(uipage);
}

/*add a new entry in the page, cloning the entry if desired (third parameter)
  in addition, it only adds non-null entries!
 this is an append operation*/
static bool efind_hilbertnode_add_entry(UIPage *uipage, void *entry, bool clone) {
    UIPage_HilbertRNode *r = (void*) uipage;

    /*the space was not previously allocated, then we should call realloc*/
    if (r->hilbertnode->type == HILBERT_INTERNAL_NODE) {
        HilbertIEntry *ent = (HilbertIEntry*) entry;

        if (ent != NULL && ent->bbox != NULL) {
            r->hilbertnode->entries.internal = (HilbertIEntry**)
                    lwrealloc(r->hilbertnode->entries.internal,
                    (r->hilbertnode->nofentries + 1) * sizeof (HilbertIEntry*));

            if (clone) {
                r->hilbertnode->entries.internal[r->hilbertnode->nofentries] = hilbertientry_clone(ent);
            } else {
                r->hilbertnode->entries.internal[r->hilbertnode->nofentries] = ent;
            }

            r->hilbertnode->nofentries++;

            return true;
        } else {
            return false;
        }
    } else {
        REntry *rentry = (REntry*) entry;

        if (entry != NULL && rentry->bbox != NULL) {
            r->hilbertnode->entries.leaf = (REntry**)
                    lwrealloc(r->hilbertnode->entries.leaf,
                    (r->hilbertnode->nofentries + 1) * sizeof (REntry*));

            if (clone) {
                r->hilbertnode->entries.leaf[r->hilbertnode->nofentries] = rentry_clone(rentry);
            } else {
                r->hilbertnode->entries.leaf[r->hilbertnode->nofentries] = rentry;
            }

            r->hilbertnode->nofentries++;

            return true;
        } else {
            return false;
        }
    }
}

/*set an entry into an existing position of the page 
 * cloning the entry if desired (third parameter)
 * freeing the old entry if desired (forth parameter)
 * in addition, it only adds non-null entries!
 */
static bool efind_hilbertnode_set_entry(UIPage *uipage, void *new_entry, int pos, bool clone, bool free_old_entry) {
    UIPage_HilbertRNode *r = (void*) uipage;

    if (r->hilbertnode->type == HILBERT_INTERNAL_NODE) {
        HilbertIEntry *hentry = (HilbertIEntry*) new_entry;
        if (pos >= r->hilbertnode->nofentries) {
            return false; //invalid position
        }
        if (new_entry != NULL && hentry->bbox != NULL) {
            if (free_old_entry) {
                lwfree(r->hilbertnode->entries.internal[pos]->bbox);
                lwfree(r->hilbertnode->entries.internal[pos]);
            }
            if (clone) {
                r->hilbertnode->entries.internal[pos] = hilbertientry_clone(hentry);
            } else {
                r->hilbertnode->entries.internal[pos] = hentry;
            }
            return true;
        } else {
            return false;
        }
    } else {
        REntry *rentry = (REntry*) new_entry;
        if (pos >= r->hilbertnode->nofentries) {
            return false; //invalid position
        }
        if (new_entry != NULL && rentry->bbox != NULL) {
            if (free_old_entry) {
                lwfree(r->hilbertnode->entries.leaf[pos]->bbox);
                lwfree(r->hilbertnode->entries.leaf[pos]);
            }
            if (clone) {
                r->hilbertnode->entries.leaf[pos] = rentry_clone(rentry);
            } else {
                r->hilbertnode->entries.leaf[pos] = rentry;
            }
            return true;
        } else {
            return false;
        }
    }

}

/*get the number of entries in the page*/
static int efind_hilbertnode_get_numberofentries(const UIPage *uipage) {
    if (uipage != NULL) {
        const UIPage_HilbertRNode *r = (void*) uipage;
        return r->hilbertnode->nofentries;
    } else {
        return 0;
    }
}

/*get a void pointer pointing to a specific entry*/
static void* efind_hilbertnode_get_entry_at(UIPage *uipage, int position) {
    UIPage_HilbertRNode *r = (void*) uipage;
    if (r == NULL || position >= r->hilbertnode->nofentries) {
        return NULL; //invalid position
    }
    if (r->hilbertnode->type == HILBERT_INTERNAL_NODE)
        return (void*) r->hilbertnode->entries.internal[position];
    else
        return (void*) r->hilbertnode->entries.leaf[position];
}

/*get the pointer of a specific entry*/
static int efind_hilbertnode_get_pointerofentry_at(const UIPage *uipage, int position) {
    const UIPage_HilbertRNode *r = (void*) uipage;
    if (r == NULL || position >= r->hilbertnode->nofentries) {
        return -1; //invalid position (change this value after...)
    }
    if (r->hilbertnode->type == HILBERT_INTERNAL_NODE)
        return r->hilbertnode->entries.internal[position]->pointer;
    else
        return r->hilbertnode->entries.leaf[position]->pointer;
}

/*clone the page, returning a void pointer that corresponds to the original page type of the underlying index*/
static void* efind_hilbertnode_clone_page(const UIPage *uipage) {
    const UIPage_HilbertRNode *r = (void*) uipage;
    return (void*) hilbertnode_clone(r->hilbertnode);
}

/*get the points to the page of the underlying index*/
static void* efind_hilbertnode_get_page(const UIPage *uipage) {
    const UIPage_HilbertRNode *r = (void*) uipage;
    return (void*) r->hilbertnode;
}

/*get the size in bytes of the current page*/
static size_t efind_hilbertnode_get_size(const UIPage *uipage) {
    const UIPage_HilbertRNode *r = (void*) uipage;
    return hilbertnode_size(r->hilbertnode);
}

/*copy from source to destination (the original page type)*/
static void efind_hilbertnode_copy_page(UIPage *dest_uipage, const UIPage *source_uipage) {
    const UIPage_HilbertRNode *source = (void*) source_uipage;
    UIPage_HilbertRNode *dest = (void*) dest_uipage;
    hilbertnode_copy(dest->hilbertnode, source->hilbertnode);
}

/*get an entry (in the form of UIEntry) of an UIPage */
static UIEntry *efind_hilbertnode_get_uientry(UIPage *uipage, int j) {
    UIPage_HilbertRNode *r = (void*) uipage;
    if (r->hilbertnode->type == HILBERT_INTERNAL_NODE)
        return efind_entryhandler_create_for_hilbertentry(r->hilbertnode->entries.internal[j]);
    else
        return efind_entryhandler_create_for_rentry(r->hilbertnode->entries.leaf[j]);
}

/*free allocated memory*/
static void efind_hilbertentry_destroy(UIEntry *uientry) {
    UIEntry_HilbertIEntry *r = (void*) uientry;
    if (r->hentry) {
        if (r->hentry->bbox)
            lwfree(r->hentry->bbox);

        lwfree(r->hentry);
    }
    lwfree(r);
}

/*get the pointer of an entry*/
static int efind_hilbertentry_get_pointer(const UIEntry *uientry) {
    const UIEntry_HilbertIEntry *r = (void*) uientry;
    return r->hentry->pointer;
}

/*get the pointer to the entry*/
static void *efind_hilbertentry_get(UIEntry *uientry) {
    UIEntry_HilbertIEntry *r = (void*) uientry;
    return (void*) r->hentry;
}

/*get the size of an entry*/
static size_t efind_hilbertentry_size(const UIEntry *uientry) {
    const UIEntry_HilbertIEntry *r = (void*) uientry;
    if (r->hentry == NULL)
        return 0;
    if (r->hentry && r->hentry->bbox) {
        return hilbertientry_size();
    } else {
        return sizeof (uint32_t); //the pointer   
    }
}

UIPage *efind_pagehandler_create_for_hilbertnode(HilbertRNode *node) {
    UIPage_HilbertRNode *ret;

    static const UIPageInterface vtable = {efind_hilbertnode_destroy,
        efind_hilbertnode_add_entry, efind_hilbertnode_set_entry, efind_hilbertnode_get_numberofentries,
        efind_hilbertnode_get_entry_at, efind_hilbertnode_get_pointerofentry_at, efind_hilbertnode_clone_page,
        efind_hilbertnode_get_page, efind_hilbertnode_get_size, efind_hilbertnode_copy_page, efind_hilbertnode_get_uientry};

    static UIPage base = {&vtable};

    ret = (UIPage_HilbertRNode*) lwalloc(sizeof (UIPage_HilbertRNode));
    memcpy(&ret->base, &base, sizeof (base));
    ret->hilbertnode = node;

    return &ret->base;
}

UIPage *efind_pagehandler_create_empty_for_hilbertnode(int nofentries, int height) {
    UIPage_HilbertRNode *ret;

    static const UIPageInterface vtable = {efind_hilbertnode_destroy,
        efind_hilbertnode_add_entry, efind_hilbertnode_set_entry, efind_hilbertnode_get_numberofentries,
        efind_hilbertnode_get_entry_at, efind_hilbertnode_get_pointerofentry_at, efind_hilbertnode_clone_page,
        efind_hilbertnode_get_page, efind_hilbertnode_get_size, efind_hilbertnode_copy_page, efind_hilbertnode_get_uientry};

    static UIPage base = {&vtable};

    ret = (UIPage_HilbertRNode*) lwalloc(sizeof (UIPage_HilbertRNode));
    memcpy(&ret->base, &base, sizeof (base));

    if (height > 0) {
        ret->hilbertnode = hilbertnode_create_empty(HILBERT_INTERNAL_NODE);
        if (nofentries > 0) {
            ret->hilbertnode->nofentries = nofentries;
            ret->hilbertnode->entries.internal = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * nofentries);
        }
    } else {
        ret->hilbertnode = hilbertnode_create_empty(HILBERT_LEAF_NODE);
        if (nofentries > 0) {
            ret->hilbertnode->nofentries = nofentries;
            ret->hilbertnode->entries.leaf = (REntry**) lwalloc(sizeof (REntry*) * nofentries);
        }
    }

    return &ret->base;
}

UIPage *efind_pagehandler_create_clone_for_hilbertnode(HilbertRNode *rnode) {
    UIPage_HilbertRNode *ret;

    static const UIPageInterface vtable = {efind_hilbertnode_destroy,
        efind_hilbertnode_add_entry, efind_hilbertnode_set_entry, efind_hilbertnode_get_numberofentries,
        efind_hilbertnode_get_entry_at, efind_hilbertnode_get_pointerofentry_at, efind_hilbertnode_clone_page,
        efind_hilbertnode_get_page, efind_hilbertnode_get_size, efind_hilbertnode_copy_page, efind_hilbertnode_get_uientry};

    static UIPage base = {&vtable};

    ret = (UIPage_HilbertRNode*) lwalloc(sizeof (UIPage_HilbertRNode));
    memcpy(&ret->base, &base, sizeof (base));

    ret->hilbertnode = hilbertnode_clone(rnode);

    return &ret->base;
}

UIEntry *efind_entryhandler_create_for_hilbertentry(HilbertIEntry *rentry) {
    UIEntry_HilbertIEntry *ret;

    static const UIEntryInterface vtable = {efind_hilbertentry_destroy, efind_hilbertentry_get_pointer,
        efind_hilbertentry_get, efind_hilbertentry_size};

    static UIEntry base = {&vtable};

    ret = (UIEntry_HilbertIEntry*) lwalloc(sizeof (UIEntry_HilbertIEntry));
    memcpy(&ret->base, &base, sizeof (base));
    ret->hentry = rentry;

    return &ret->base;
}
