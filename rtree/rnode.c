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
#include "rnode.h"

#include <string.h> /* for memcpy */
#include <malloc.h> /* for posix_memalign */
#include <stringbuffer.h> /* for stringbuffer of postgis */
#include <lwgeom_geos.h> /* for lwgeom_union and so on*/
#include <lwgeom_log.h> //because of lwnotice (for GEOS)
#include "../main/storage_handler.h" /* to write/read nodes */
#include "../main/math_util.h" /*for compartions between double values */ 
#include "../main/log_messages.h" /* error and warning messages */
#include "../main/io_handler.h" /*for direct or normal access */

void rnode_add_rentry(RNode *node, REntry *entry) {
    /*check it only in debug mode*/
    /*
    int i = 0;
    if (node->nofentries > 1) {
        for (i = 0; i < node->nofentries; i++) {
            if (node->entries[i] == entry || node->entries[i]->pointer == entry->pointer) {
                _DEBUGF(ERROR, "Found duplicate entry in a node %p == %p", node->entries[i], entry);
                return;
            }
        }
    }*/
    //if we don't have entries, we alloc some initial space
    if (node->entries == NULL) {
        node->entries = (REntry**) lwalloc(1 * sizeof (REntry*));
    } else {
        //otherwise, we need to realloc
        node->entries = (REntry**) lwrealloc(node->entries, (node->nofentries + 1) * sizeof (REntry*));
    }
    if (node->entries == NULL)
        _DEBUGF(ERROR, "Error in to resize the entries (%d) of a node", node->nofentries);

    node->entries[node->nofentries] = entry;
    node->nofentries++;
}

void rnode_remove_rentry(RNode *node, int entry) {
    if (entry < 0 || entry >= node->nofentries) {
        _DEBUGF(ERROR, "Entry %d does not exist and cannot be removed (size of node = %d).",
                entry, node->nofentries);
    } else {
        /*tested with valgrind and it is OK!*/
        if (entry < node->nofentries - 1) {
            lwfree(node->entries[entry]->bbox);
            lwfree(node->entries[entry]);
            node->entries[entry] = NULL;
            memmove(node->entries + entry, node->entries + (entry + 1), sizeof (node->entries[0]) * (node->nofentries - entry - 1));
        } else if (entry == node->nofentries - 1) {
            //if this is the last element, we only remove it from the memory
            lwfree(node->entries[entry]->bbox);
            lwfree(node->entries[entry]);
        }

        /* We have one less point */
        node->nofentries--;
    }
}

/* copy an entry and return its pointer */
REntry *rentry_clone(const REntry *entry) {
    REntry *copied = NULL;
    BBox *b = NULL;

    copied = (REntry*) lwalloc(sizeof (REntry));
    b = (BBox*) lwalloc(sizeof (BBox));

    copied->pointer = entry->pointer;
    copied->bbox = b;

    memcpy(copied->bbox, entry->bbox, sizeof (BBox));
    return copied;
}

/* copy rnode and return its pointer */
RNode *rnode_clone(const RNode *rnode) {
    int i;
    RNode *cloned = (RNode*) lwalloc(sizeof (RNode));
    cloned->nofentries = rnode->nofentries;
    cloned->entries = (REntry**) lwalloc(sizeof (REntry*) * cloned->nofentries);
    for (i = 0; i < rnode->nofentries; i++) {
        cloned->entries[i] = rentry_clone(rnode->entries[i]);
    }
    return cloned;
}

void rnode_copy(RNode *dest, const RNode *src) {
    int i;
    int old;
    old = dest->nofentries;
    dest->nofentries = src->nofentries;

    /*tested with valgrind and it is OK!*/
    if (dest->entries == NULL) {
        dest->entries = (REntry**) lwalloc(sizeof (REntry*) * src->nofentries);
    } else if (old < src->nofentries) {
        dest->entries = (REntry**) lwrealloc(dest->entries, sizeof (REntry*) * src->nofentries);
    } else if (old > src->nofentries) {
        int del = 0;
        for (i = old - 1; i >= src->nofentries; i--) {
            del++;
            if (i < old - 1) {
                lwfree(dest->entries[i]->bbox);
                lwfree(dest->entries[i]);
                dest->entries[i] = NULL;
                memmove(dest->entries + i, dest->entries + (i + 1), sizeof (dest->entries[0]) * (old - i - 1));
            } else if (i == (old - 1)) {
                //if this is the last element, we only remove it from the memory
                lwfree(dest->entries[i]->bbox);
                lwfree(dest->entries[i]);
            }
        }
        if (del + src->nofentries != old) {
            _DEBUG(ERROR, "Wow, we removed extra entries in rnode_copy");
        }
    }

    for (i = 0; i < src->nofentries; i++) {
        if (i >= old) {
            dest->entries[i] = (REntry*) lwalloc(sizeof (REntry));
            dest->entries[i]->bbox = (BBox*) lwalloc(sizeof (BBox));
        }
        dest->entries[i]->pointer = src->entries[i]->pointer;
        memcpy(dest->entries[i]->bbox, src->entries[i]->bbox, sizeof (BBox));
    }
}

/* compute the BBOX of a node (on all its entries) */
BBox *rnode_compute_bbox(const RNode *node) {
    int i;
    int j;
    BBox *bbox = bbox_create();

    if (node->entries == NULL || node->nofentries == 0)
        _DEBUG(ERROR, "There is no entry in the current node in compute_bbox_of_node");

    for (i = 0; i <= MAX_DIM; i++) {
        bbox->max[i] = node->entries[0]->bbox->max[i];
        bbox->min[i] = node->entries[0]->bbox->min[i];
    }

    for (j = 1; j < node->nofentries; j++) {
        for (i = 0; i <= MAX_DIM; i++) {
            bbox->max[i] = DB_MAX(bbox->max[i], node->entries[j]->bbox->max[i]);
            bbox->min[i] = DB_MIN(bbox->min[i], node->entries[j]->bbox->min[i]);
        }
    }
    return bbox;
}

/* create an entry */
REntry *rentry_create(int pointer, BBox *bbox) {
    REntry *entry = (REntry*) lwalloc(sizeof (REntry));
    entry->pointer = pointer;
    entry->bbox = bbox;
    return entry;
}

/* create an empty RNODE*/
RNode *rnode_create_empty() {
    RNode *r = (RNode*) lwalloc(sizeof (RNode));
    r->nofentries = 0;
    r->entries = NULL; //there is no entry in this node
    return r;
}

/*return the size in bytes of a rnode instance*/
size_t rnode_size(const RNode *node) {
    size_t size = 0;
    size += sizeof (uint32_t); //nofentries
    size += rentry_size() * node->nofentries;
    return size;
}

/*return the size in bytes of a rentry */
size_t rentry_size() {
    int size = 0;
    size += sizeof (uint32_t); //the pointer    
    size += sizeof(BBox);//sizeof (double) * NUM_OF_DIM * 2; //the bbox
    return size; // = 4+(8*2*2) = 36
}

void rnode_free(RNode *node) {
    if (node) {
        if (node->entries && node->nofentries > 0) {
            int i;
            for (i = 0; i < node->nofentries; i++) {
                if (node->entries[i]->bbox) {
                    lwfree(node->entries[i]->bbox);
                }
                lwfree(node->entries[i]);
            }
            lwfree(node->entries);
        }
        lwfree(node);
    }
}

void rentry_free(REntry *entry) {
    if (entry) {
        if (entry->bbox)
            lwfree(entry->bbox);
        lwfree(entry);
    }
}

/* read a node from the file or buffer */
RNode *get_rnode(const SpatialIndex *si, int page_num, int height) {
    RNode *node = rnode_create_empty();
    int i;
    uint8_t *buf;
    uint8_t *loc;

    if (si->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, si->gp->page_size, si->gp->page_size)) {
            _DEBUG(ERROR, "Allocation failed at get_rnode");
            return NULL;
        }
    } else {
        buf = (uint8_t*) lwalloc(si->gp->page_size);
    }

    //we recover the requested node
    storage_read_one_page(si, page_num, buf, height);

    loc = buf;

    /* now we have to deserialize the buf*/
    memcpy(&node->nofentries, loc, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    if (node->nofentries == 0) {
        if (page_num != 0) {
            /*we allows this situation since a flushing operation can choose 
             * an empty rnode to be flushed
             * so, if we need to get this rnode back, this will be again an empty node
             * however, this is not a good situation; therefore, we show this warning
             * */
            _DEBUGF(WARNING, "It reads an empty node at %d page in get_node "
                    "and it is not an empty index", page_num);
            //return NULL;
        }
        node->entries = NULL;
    } else {
        node->entries = (REntry**) lwalloc(sizeof (REntry*) * node->nofentries);
    }

    for (i = 0; i < node->nofentries; i++) {
        node->entries[i] = (REntry*) lwalloc(sizeof (REntry));
        node->entries[i]->bbox = (BBox*) lwalloc(sizeof (BBox));

        memcpy(&(node->entries[i]->pointer), loc, sizeof (uint32_t));
        loc += sizeof (uint32_t);

        memcpy(node->entries[i]->bbox, loc, sizeof (BBox));
        loc += sizeof (BBox);
    }

    if (si->gp->io_access == DIRECT_ACCESS) {
        free(buf);
    } else {
        lwfree(buf);
    }
    return node;
}

/* write the node to file */
void put_rnode(const SpatialIndex *si, const RNode *node, int page_num, int height) {
    uint8_t *loc;
    uint8_t *buf;
    int i;

    if (si->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, si->gp->page_size, si->gp->page_size)) {
            _DEBUG(ERROR, "Allocation failed at put_rnode");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(si->gp->page_size);
    }

    loc = buf;

    /* now we have to serialize the node into the buf*/
    memcpy(loc, &node->nofentries, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    for (i = 0; i < node->nofentries; i++) {
        memcpy(loc, &(node->entries[i]->pointer), sizeof (uint32_t));
        loc += sizeof (uint32_t);

        memcpy(loc, node->entries[i]->bbox, sizeof (BBox));
        loc += sizeof (BBox);
    }

    //we store the node
    storage_write_one_page(si, buf, page_num, height);

    if (si->gp->io_access == DIRECT_ACCESS) {
        free(buf);
    } else {
        lwfree(buf);
    }
}

/* delete the node from file */
void del_rnode(const SpatialIndex *si, int page_num, int height) {
    uint8_t *loc;
    uint8_t *buf;
    int inv = -1;

    if (si->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, si->gp->page_size, si->gp->page_size)) {
            _DEBUG(ERROR, "Allocation failed at get_rnode");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(si->gp->page_size);
    }

    loc = buf;

    /*we serialize an invalid node here*/
    memcpy(loc, &inv, sizeof (int32_t));
    loc += sizeof (int32_t);

    storage_write_one_page(si, buf, page_num, height);

    if (si->gp->io_access == DIRECT_ACCESS) {
        free(buf);
    } else {
        lwfree(buf);
    }
}

void rnode_serialize(const RNode *node, uint8_t *buf) {
    uint8_t *loc;
    int i;
    loc = buf;
    if (node == NULL) {
        int inv = -1;
        /*we serialize an invalid node here*/
        memcpy(loc, &inv, sizeof (int32_t));
        loc += sizeof (int32_t);
    } else {
        /* now we have to serialize the node into the buf*/
        memcpy(loc, &(node->nofentries), sizeof (uint32_t));
        loc += sizeof (uint32_t);

        for (i = 0; i < node->nofentries; i++) {
            memcpy(loc, &(node->entries[i]->pointer), sizeof (uint32_t));
            loc += sizeof (uint32_t);

            memcpy(loc, node->entries[i]->bbox, sizeof (BBox));
            loc += sizeof (BBox);
        }
    }
}

/*compute the dead space area of a rnode */
double rnode_dead_space_area(const RNode *node) {
    BBox* bbox;
    double deadspace = 0.0;
    LWGEOM *aux;
    GEOSGeometry *g;
    GEOSGeometry *un;
    GEOSGeometry *temp;
    int i;

    initGEOS(lwnotice, lwgeom_geos_error);

    if (node->nofentries >= 2) {
        aux = bbox_to_geom(node->entries[0]->bbox);
        un = LWGEOM2GEOS(aux, 0);
        lwgeom_free(aux);

        for (i = 1; i < node->nofentries; i++) {
            aux = bbox_to_geom(node->entries[i]->bbox);
            g = LWGEOM2GEOS(aux, 0);
            lwgeom_free(aux);

            temp = GEOSUnion(un, g);
            GEOSGeom_destroy(un);
            GEOSGeom_destroy(g);
            un = temp;
            temp = NULL;
        }

        bbox = rnode_compute_bbox(node);
        aux = bbox_to_geom(bbox);
        g = LWGEOM2GEOS(aux, 0);
        lwgeom_free(aux);
        lwfree(bbox);

        temp = GEOSDifference(g, un);

        GEOSArea(temp, &deadspace);

        GEOSGeom_destroy(temp);
        GEOSGeom_destroy(g);
        GEOSGeom_destroy(un);
    }
    return deadspace;
}

/* compute the overlapping area of a rnode */
double rnode_overlapping_area(const RNode *node) {
    int i;
    int j;
    double ovp_area = 0.0;

    for (i = 0; i < node->nofentries; i++) {
        for (j = 0; j < node->nofentries; j++) {
            if (i != j) {
                if (bbox_check_predicate(node->entries[i]->bbox, node->entries[j]->bbox, INTERSECTS))
                    ovp_area += bbox_overlap_area(node->entries[i]->bbox, node->entries[j]->bbox);
            }
        }
    }
    return ovp_area;
}

/* compute the overlapping area of a set of entries */
double rentries_overlapping_area(const REntry **entries, int n) {
    int i;
    int j;
    double ovp_area = 0.0;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (i != j) {
                if (bbox_check_predicate(entries[i]->bbox, entries[j]->bbox, INTERSECTS))
                    ovp_area += bbox_overlap_area(entries[i]->bbox, entries[j]->bbox);
            }
        }
    }
    return ovp_area;
}

/* compute the margin of a set of entries */
double rentry_margin(const REntry **entries, int n) {
    int i, j;
    BBox *bbox = bbox_create();
    double margin = 0.0;

    if (entries == NULL || n == 0)
        _DEBUG(ERROR, "There is no entry to compute the margin");

    for (i = 0; i <= MAX_DIM; i++) {
        bbox->max[i] = entries[0]->bbox->max[i];
        bbox->min[i] = entries[0]->bbox->min[i];
    }

    for (j = 1; j < n; j++) {
        for (i = 0; i <= MAX_DIM; i++) {
            bbox->max[i] = DB_MAX(bbox->max[i], entries[j]->bbox->max[i]);
            bbox->min[i] = DB_MIN(bbox->min[i], entries[j]->bbox->min[i]);
        }
    }

    for (i = 0; i <= MAX_DIM; i++) {
        margin += bbox->max[i] - bbox->min[i];
    }
    lwfree(bbox);
    return margin;
}

/*set the coordinates of a bbox by considering a set of entries (union of all these entries)*/
void rentry_create_bbox(const REntry **entries, int n, BBox *un) {
    int i, j;

    if (entries == NULL || n == 0)
        _DEBUG(ERROR, "There is no entry to compute the margin");

    if (un == NULL)
        _DEBUG(ERROR, "There is no bbox (bbox is null) in rentry_create_bbox");

    for (i = 0; i <= MAX_DIM; i++) {
        un->max[i] = entries[0]->bbox->max[i];
        un->min[i] = entries[0]->bbox->min[i];
    }

    for (j = 1; j < n; j++) {
        for (i = 0; i <= MAX_DIM; i++) {
            un->max[i] = DB_MAX(un->max[i], entries[j]->bbox->max[i]);
            un->min[i] = DB_MIN(un->min[i], entries[j]->bbox->min[i]);
        }
    }
}

void rnode_print(const RNode *node, int node_id) {
    int i;
    char *print;
    stringbuffer_t *sb;

    sb = stringbuffer_create();
    stringbuffer_append(sb, "RNODE(");
    stringbuffer_append(sb, "number of elements = ");
    stringbuffer_aprintf(sb, "%d, and size is %d bytes => ( ", node->nofentries, rnode_size(node));
    for (i = 0; i < node->nofentries; i++) {
        stringbuffer_aprintf(sb, "(pointer %d - bbox min/max %f, %f, %f, %f)  ",
                node->entries[i]->pointer, node->entries[i]->bbox->min[0],
                node->entries[i]->bbox->min[1],
                node->entries[i]->bbox->max[0],
                node->entries[i]->bbox->max[1]);
    }
    stringbuffer_append(sb, ")");
    print = stringbuffer_getstringcopy(sb);
    stringbuffer_destroy(sb);

    _DEBUGF(NOTICE, "NODE_ID: %d, CONTENT: %s", node_id, print);

    lwfree(print);
}
