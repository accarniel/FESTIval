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
 * File:   efind_page_handler.h
 * Author: anderson
 *
 * Created on September 17, 2017, 10:36 AM
 */

#ifndef EFIND_PAGE_HANDLER_H
#define EFIND_PAGE_HANDLER_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* This file is responsible to define the Underlying Index Page (UIPage) and Underlying Index Entry (UIEntry) structures, which are used by eFIND
 We use the notion of polymorphism here in order to provide generality
 Thus, general functions are linked here avoiding a lot of IFs in the code*/

/*this struct defines an UIEntry data type (used only by the Red-Black Tree)
 this means that UIEntry is a generic page that store entries of the index
 * it allows to multiple dynamic dispatching (polymorphism)
 */
typedef struct {
    const struct _UIEntryInterface * const vtable;
} UIEntry;

typedef struct _UIEntryInterface {
    /*free allocated memory*/
    void (*destroy)(UIEntry *uientry);

    /*get the pointer of an entry*/
    int (*get_pointer)(const UIEntry *uientry);
    /*get the pointer itself*/
    void* (*get)(UIEntry *uientry);
    /*the size of an entry*/
    size_t (*size)(const UIEntry *uientry);
} UIEntryInterface;

/*this struct defines an UIPage data type (used by the read buffer, write buffer, and red-black tree)
 this means that UIPage is a generic page that store entries of the index
 * it allows to multiple dynamic dispatching (polymorphism)
 */
typedef struct {
    const struct _UIPageInterface * const vtable;
} UIPage;

typedef struct _UIPageInterface {
    /*free allocated memory*/
    void (*destroy)(UIPage *uipage);
    /*add a new entry in the page, cloning the entry if desired (third parameter)*/
    bool (*add_entry)(UIPage *uipage, void *entry, bool clone);
    /*set an entry into an existing position of the page 
     * cloning the entry if desired (third parameter)
     * freeing the old entry if desired (forth parameter)
     */
    bool (*set_entry)(UIPage *uipage, void *new_entry, int pos, bool clone, bool free_old_entry);
    /*get the number of entries in the page*/
    int (*get_numberofentries)(const UIPage *uipage);
    /*get a void pointer pointing to a specific entry*/
    void* (*get_entry_at)(UIPage *uipage, int position);
    /*get the pointer of a specific entry*/
    int (*get_pointerofentry_at)(const UIPage *uipage, int position);
    /*clone the page, returning a void pointer that corresponds to the original page type of the underlying index*/
    void* (*clone_page)(const UIPage *uipage);
    /*get the points to the page of the underlying index*/
    void* (*get_page)(const UIPage *uipage);
    /*get the size in bytes of the current page*/
    size_t (*get_size)(const UIPage *uipage);
    /*copy from source to destination (the original page type)*/
    void (*copy_page)(UIPage *dest_uipage, const UIPage *source_uipage);
    /*get an entry (in the form of UIEntry) of an UIPage */
    UIEntry* (*get_uientry_at)(UIPage *u, int p);
} UIPageInterface;

/************************************
             WRAPPERS FOR UIPage
 ************************************/

static inline void efind_pagehandler_destroy(UIPage *u) {
    u->vtable->destroy(u);
}

static inline bool efind_pagehandler_add_entry(UIPage *u, void *e, bool c) {
    return u->vtable->add_entry(u, e, c);
}

static inline bool efind_pagehandler_set_entry(UIPage *u, void *ne, int pos, bool c, bool f) {
    return u->vtable->set_entry(u, ne, pos, c, f);
}

static inline int efind_pagehandler_get_nofentries(const UIPage *u) {
    return u->vtable->get_numberofentries(u);
}

static inline void *efind_pagehandler_get_entry_at(UIPage *u, int p) {
    return u->vtable->get_entry_at(u, p);
}

static inline UIEntry *efind_pagehandler_get_uientry_at(UIPage *u, int p) {
    return u->vtable->get_uientry_at(u, p);
}

static inline int efind_pagehandler_get_pofentry_at(const UIPage *u, int p) {
    return u->vtable->get_pointerofentry_at(u, p);
}

static inline void *efind_pagehandler_get_clone(const UIPage *u) {
    return u->vtable->clone_page(u);
}

static inline void *efind_pagehandler_get(const UIPage *u) {
    return u->vtable->get_page(u);
}

static inline size_t efind_pagehandler_get_size(const UIPage *u) {
    return u->vtable->get_size(u);
}

static inline void efind_pagehandler_copy_page(UIPage *u, const UIPage *s) {
    return u->vtable->copy_page(u, s);
}

/************************************
             WRAPPERS FOR UIEntry
 ************************************/

static inline void efind_entryhandler_destroy(UIEntry *u) {
    u->vtable->destroy(u);
}

static inline int efind_entryhandler_get_pofentry(const UIEntry *u) {
    return u->vtable->get_pointer(u);
}

static inline void *efind_entryhandler_get(UIEntry *u) {
    return u->vtable->get(u);
}

static inline size_t efind_entryhandler_size(const UIEntry *u) {
    return u->vtable->size(u);
}

/************************************
 Functions to Create a UIPage (according to the supported efind indices)
 ************************************/

/*this function creates an UIPage from a page/node of a supported underlying index*/
extern UIPage *efind_pagehandler_create(void *p, uint8_t index_type);
/*this function creates an UIPage with some initial allocated space (ONLY SPACE, THE ENTRIES ARE NULL)*/
extern UIPage *efind_pagehandler_create_empty(int nofentries, int height, uint8_t index_type);
/*this function creates an UIPage from a node, cloning its content*/
extern UIPage *efind_pagehandler_create_clone(void *p, uint8_t index_type);

/************************************
 Functions to Create a UIEntry (according to the supported efind indices)
 ************************************/
//additional parameter should be passed in param
extern UIEntry *efind_entryhandler_create(void *e, uint8_t index_type, const void *param);

#endif /* EFIND_PAGE_HANDLER_H */

