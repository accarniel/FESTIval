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

#include "hilbert_node.h"
#include <string.h> /* for memcpy */
#include <malloc.h> /* for posix_memalign */
#include <stringbuffer.h> /* for stringbuffer of postgis */
#include <lwgeom_geos.h> /* for lwgeom_union and so on*/
#include <lwgeom_log.h> //because of lwnotice (for GEOS)
#include "../main/math_util.h" /*for compartions between double values */
#include "../main/io_handler.h" /*for direct or normal access */
#include "../main/storage_handler.h" /* to write/read nodes */

#include "../main/log_messages.h"
#include "hilbert_value.h"

/*insert an entry into a node, respecting the hilbert value (it does not copy the entry, it only add its reference)*/
int hilbertnode_add_entry(HilbertRNode *node, void *entry, hilbert_value_t h, int srid) {
    int pos = -1;
    if (node->type == HILBERT_LEAF_NODE) {
        //if we don't have entries, we alloc some initial space
        if (node->entries.leaf == NULL) {
            node->entries.leaf = (REntry**) lwalloc(1 * sizeof (REntry*));
            node->entries.leaf[node->nofentries] = (REntry*) entry;

            pos = node->nofentries;
        } else {
            hilbert_value_t cur;
            int i = 1;
            //otherwise, we need to realloc and insert respecting the hilbert order
            node->entries.leaf = (REntry**) lwrealloc(node->entries.leaf, (node->nofentries + 1) * sizeof (REntry*));
            if (node->entries.leaf == NULL)
                _DEBUGF(ERROR, "Error in to resize the entries (%d) of a node", node->nofentries);

            for (i = 0; i < node->nofentries; i++) {
                cur = hilbertvalue_compute(node->entries.leaf[i]->bbox, srid);
                if (h < cur) {
                    //we should insert the new entry in i 
                    memmove(node->entries.leaf + (i + 1), node->entries.leaf + i, sizeof (node->entries.leaf[0]) * (node->nofentries - i));
                    node->entries.leaf[i] = (REntry*) entry;
                    pos = i;
                    node->nofentries++;
                    return pos;
                }
            }
            //if it was not inserted before, this means that h is the largest hilbert value
            node->entries.leaf[node->nofentries] = (REntry*) entry;
            pos = node->nofentries;
        }

    } else {
        //if we don't have entries, we alloc some initial space
        if (node->entries.internal == NULL) {
            node->entries.internal = (HilbertIEntry**) lwalloc(1 * sizeof (HilbertIEntry*));
            node->entries.internal[node->nofentries] = (HilbertIEntry*) entry;
            pos = node->nofentries;
        } else {
            hilbert_value_t cur;
            int i = 1;
            //otherwise, we need to realloc and insert respecting the hilbert order
            node->entries.internal = (HilbertIEntry**) lwrealloc(node->entries.internal, (node->nofentries + 1) * sizeof (HilbertIEntry*));
            if (node->entries.internal == NULL)
                _DEBUGF(ERROR, "Error in to resize the entries (%d) of a node", node->nofentries);

            for (i = 0; i < node->nofentries; i++) {
                cur = node->entries.internal[i]->lhv;
                if (h < cur) {
                    //we should insert the new entry in i 
                    memmove(node->entries.internal + (i + 1), node->entries.internal + i,
                            sizeof (node->entries.internal[0]) * (node->nofentries - i));
                    node->entries.internal[i] = (HilbertIEntry*) entry;
                    pos = i;
                    node->nofentries++;
                    return pos;
                }
            }
            //if it was not inserted before, this means that the h is the largest hilbert value
            node->entries.internal[node->nofentries] = (HilbertIEntry*) entry;
            pos = node->nofentries;
        }
    }

    node->nofentries++;

    return pos;
}

/*remove an entry from a node*/
void hilbertnode_remove_entry(HilbertRNode *node, int entry) {
    if (entry < 0 || entry >= node->nofentries) {
        _DEBUGF(ERROR, "Entry %d does not exist and cannot be removed (size of node = %d).",
                entry, node->nofentries);
    } else {
        if (node->type == HILBERT_LEAF_NODE) {
            if (entry < node->nofentries - 1) {
                rentry_free(node->entries.leaf[entry]);
                node->entries.leaf[entry] = NULL;
                memmove(node->entries.leaf + entry, node->entries.leaf + (entry + 1),
                        sizeof (node->entries.leaf[0]) * (node->nofentries - entry - 1));
            } else if (entry == node->nofentries - 1) {
                //if this is the last element, we only remove it from the memory
                rentry_free(node->entries.leaf[entry]);
            }
        } else {
            if (entry < node->nofentries - 1) {
                hilbertentry_free(node->entries.internal[entry]);
                node->entries.internal[entry] = NULL;
                memmove(node->entries.internal + entry, node->entries.internal + (entry + 1),
                        sizeof (node->entries.internal[0]) * (node->nofentries - entry - 1));
            } else if (entry == node->nofentries - 1) {
                //if this is the last element, we only remove it from the memory
                hilbertentry_free(node->entries.internal[entry]);
            }
        }
        /* We have one less point */
        node->nofentries--;
    }
}

/* copy an internal entry and return its pointer */
HilbertIEntry *hilbertientry_clone(const HilbertIEntry *entry) {
    HilbertIEntry *copied = NULL;
    BBox *b = NULL;

    copied = (HilbertIEntry*) lwalloc(sizeof (HilbertIEntry));
    b = (BBox*) lwalloc(sizeof (BBox));

    copied->pointer = entry->pointer;
    copied->bbox = b;
    copied->lhv = entry->lhv;

    memcpy(copied->bbox, entry->bbox, sizeof (BBox));
    return copied;
}

/* copy a node and return its pointer */
HilbertRNode *hilbertnode_clone(const HilbertRNode *node) {
    int i;
    HilbertRNode *cloned = (HilbertRNode*) lwalloc(sizeof (HilbertRNode));
    cloned->nofentries = node->nofentries;
    cloned->type = node->type;
    if (node->type == HILBERT_LEAF_NODE) {
        cloned->entries.leaf = (REntry**) lwalloc(sizeof (REntry*) * cloned->nofentries);
        for (i = 0; i < node->nofentries; i++) {
            cloned->entries.leaf[i] = rentry_clone(node->entries.leaf[i]);
        }
    } else {
        cloned->entries.internal = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * cloned->nofentries);
        for (i = 0; i < node->nofentries; i++) {
            cloned->entries.internal[i] = hilbertientry_clone(node->entries.internal[i]);
        }
    }
    return cloned;
}

/* copy in a destination node a node*/
void hilbertnode_copy(HilbertRNode *dest, const HilbertRNode *src) {
    int i;
    int old;

    if (src->type != dest->type) {
        //we need to destroy the destination and then clone the source
        int i;
        if (dest->type == HILBERT_LEAF_NODE) {
            for (i = 0; i < dest->nofentries; i++) {
                lwfree(dest->entries.leaf[i]->bbox);
                lwfree(dest->entries.leaf[i]);
            }
            lwfree(dest->entries.leaf);
            dest->entries.leaf = NULL;
        } else {
            for (i = 0; i < dest->nofentries; i++) {
                lwfree(dest->entries.internal[i]->bbox);
                lwfree(dest->entries.internal[i]);
            }
            lwfree(dest->entries.internal);
            dest->entries.internal = NULL;
        }

        dest->nofentries = 0;
        dest->type = src->type;
        hilbertnode_copy(dest, src);
    }

    old = dest->nofentries;
    dest->nofentries = src->nofentries;

    if (src->type == HILBERT_LEAF_NODE) {
        if (dest->entries.leaf == NULL) {
            dest->entries.leaf = (REntry**) lwalloc(sizeof (REntry*) * src->nofentries);
        } else if (old < src->nofentries) {
            dest->entries.leaf = (REntry**) lwrealloc(dest->entries.leaf, sizeof (REntry*) * src->nofentries);
        } else if (old > src->nofentries) {
            int del = 0;
            for (i = old - 1; i >= src->nofentries; i--) {
                del++;
                if (i < old - 1) {
                    rentry_free(dest->entries.leaf[i]);
                    dest->entries.leaf[i] = NULL;
                    memmove(dest->entries.leaf + i, dest->entries.leaf + (i + 1), sizeof (dest->entries.leaf[0]) * (old - i - 1));
                } else if (i == (old - 1)) {
                    //if this is the last element, we only remove it from the memory
                    rentry_free(dest->entries.leaf[i]);
                }
            }
            if (del + src->nofentries != old) {
                _DEBUG(ERROR, "Wow, we removed extra entries in hilbertnode_copy");
            }
        }

        for (i = 0; i < src->nofentries; i++) {
            if (i >= old) {
                dest->entries.leaf[i] = (REntry*) lwalloc(sizeof (REntry));
                dest->entries.leaf[i]->bbox = (BBox*) lwalloc(sizeof (BBox));
            }
            dest->entries.leaf[i]->pointer = src->entries.leaf[i]->pointer;
            memcpy(dest->entries.leaf[i]->bbox, src->entries.leaf[i]->bbox, sizeof (BBox));
        }
    } else {
        if (dest->entries.internal == NULL) {
            dest->entries.internal = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * src->nofentries);
        } else if (old < src->nofentries) {
            dest->entries.internal = (HilbertIEntry**) lwrealloc(dest->entries.internal, sizeof (HilbertIEntry*) * src->nofentries);
        } else if (old > src->nofentries) {
            int del = 0;
            for (i = old - 1; i >= src->nofentries; i--) {
                del++;
                if (i < old - 1) {
                    hilbertentry_free(dest->entries.internal[i]);
                    dest->entries.internal[i] = NULL;
                    memmove(dest->entries.internal + i, dest->entries.internal + (i + 1), sizeof (dest->entries.internal[0]) * (old - i - 1));
                } else if (i == (old - 1)) {
                    //if this is the last element, we only remove it from the memory
                    hilbertentry_free(dest->entries.internal[i]);
                }
            }
            if (del + src->nofentries != old) {
                _DEBUG(ERROR, "Wow, we removed extra entries in hilbertnode_copy");
            }
        }

        for (i = 0; i < src->nofentries; i++) {
            if (i >= old) {
                dest->entries.internal[i] = (HilbertIEntry*) lwalloc(sizeof (HilbertIEntry));
                dest->entries.internal[i]->bbox = (BBox*) lwalloc(sizeof (BBox));
            }
            dest->entries.internal[i]->pointer = src->entries.internal[i]->pointer;
            dest->entries.internal[i]->lhv = src->entries.internal[i]->lhv;
            memcpy(dest->entries.internal[i]->bbox, src->entries.internal[i]->bbox, sizeof (BBox));
        }
    }
}

/* create an empty HilbertNODE*/
HilbertRNode *hilbertnode_create_empty(uint8_t type) {
    HilbertRNode *r;
    if (!(type == HILBERT_INTERNAL_NODE || type == HILBERT_LEAF_NODE))
        _DEBUGF(ERROR, "Invalid type of hilbert node %d", type);

    r = (HilbertRNode*) lwalloc(sizeof (HilbertRNode));
    r->nofentries = 0;
    r->type = type;
    if (type == HILBERT_INTERNAL_NODE) {
        r->entries.internal = NULL;
    } else {
        r->entries.leaf = NULL;
    }
    return r;
}

/* return the size of a node instance*/
size_t hilbertnode_size(const HilbertRNode *node) {
    size_t size = 0;
    size += sizeof (uint32_t); //nofentries
    size += sizeof (uint8_t); //type of node
    if (node->type == HILBERT_LEAF_NODE)
        size += rentry_size() * node->nofentries;
    else
        size += hilbertientry_size() * node->nofentries;
    return size;
}

/* return the size of an internal entry */
size_t hilbertientry_size(void) {
    return rentry_size() + sizeof (hilbert_value_t); // = 4+(8*2*2)+8 = 44
}

/* free a HilbertRNode */
void hilbertnode_free(HilbertRNode *node) {
    if (node) {
        if (node->nofentries > 0) {
            int i;
            if (node->type == HILBERT_LEAF_NODE) {
                for (i = 0; i < node->nofentries; i++) {
                    if (node->entries.leaf[i]->bbox) {
                        lwfree(node->entries.leaf[i]->bbox);
                    }
                    lwfree(node->entries.leaf[i]);
                }
                lwfree(node->entries.leaf);
            } else {
                for (i = 0; i < node->nofentries; i++) {
                    if (node->entries.internal[i]->bbox) {
                        lwfree(node->entries.internal[i]->bbox);
                    }
                    lwfree(node->entries.internal[i]);
                }
                lwfree(node->entries.internal);
            }
        }
        lwfree(node);
    }
}

/* free an entry of a HilbertRNode */
void hilbertentry_free(HilbertIEntry *entry) {
    if (entry) {
        if (entry->bbox)
            lwfree(entry->bbox);
        lwfree(entry);
    }
}

/* create an internal entry */
HilbertIEntry *hilbertentry_create(int pointer, BBox *bbox, hilbert_value_t lhv) {
    HilbertIEntry *entry = (HilbertIEntry*) lwalloc(sizeof (HilbertIEntry));
    entry->pointer = pointer;
    entry->bbox = bbox;
    entry->lhv = lhv;
    return entry;
}

/* read the node from file */
HilbertRNode *get_hilbertnode(const SpatialIndex *si, int page_num, int height) {
    HilbertRNode *node = (HilbertRNode*) lwalloc(sizeof (HilbertRNode));
    int i;
    uint8_t *buf;
    uint8_t *loc;

    if (si->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, si->gp->page_size, si->gp->page_size)) {
            _DEBUG(ERROR, "Allocation failed at get_hilbertnode");
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

    memcpy(&node->type, loc, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    if (node->nofentries == 0) {
        if (page_num != 0) {
            /*we allow this situation since a flushing operation can choose 
             * an empty rnode to be flushed
             * so, if we need to get this rnode back, this will be again an empty node
             * however, this is not a good situation; therefore, we show this warning
             * */
            _DEBUGF(WARNING, "It has read an empty node at %d page in get_hilbertnode "
                    "and it is not an empty index", page_num);
            //return NULL;
        }
    } else {
        if (node->type == HILBERT_LEAF_NODE) {
            node->entries.leaf = (REntry**) lwalloc(sizeof (REntry*) * node->nofentries);
            for (i = 0; i < node->nofentries; i++) {
                node->entries.leaf[i] = (REntry*) lwalloc(sizeof (REntry));
                node->entries.leaf[i]->bbox = (BBox*) lwalloc(sizeof (BBox));

                memcpy(&(node->entries.leaf[i]->pointer), loc, sizeof (uint32_t));
                loc += sizeof (uint32_t);

                memcpy(node->entries.leaf[i]->bbox, loc, sizeof (BBox));
                loc += sizeof (BBox);
            }
        } else {
            node->entries.internal = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * node->nofentries);
            for (i = 0; i < node->nofentries; i++) {
                node->entries.internal[i] = (HilbertIEntry*) lwalloc(sizeof (HilbertIEntry));
                node->entries.internal[i]->bbox = (BBox*) lwalloc(sizeof (BBox));

                memcpy(&(node->entries.internal[i]->pointer), loc, sizeof (uint32_t));
                loc += sizeof (uint32_t);

                memcpy(&(node->entries.internal[i]->lhv), loc, sizeof (hilbert_value_t));
                loc += sizeof (hilbert_value_t);

                memcpy(node->entries.internal[i]->bbox, loc, sizeof (BBox));
                loc += sizeof (BBox);
            }
        }
    }

    if (si->gp->io_access == DIRECT_ACCESS) {
        free(buf);
    } else {
        lwfree(buf);
    }

    return node;
}

/* write the node to file */
void put_hilbertnode(const SpatialIndex *si, const HilbertRNode *node, int page_num, int height) {
    uint8_t *loc;
    uint8_t *buf;
    int i;

    if (si->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, si->gp->page_size, si->gp->page_size)) {
            _DEBUG(ERROR, "Allocation failed at put_hilbertnode");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(si->gp->page_size);
    }

    loc = buf;

    /* now we have to serialize the node into the buf*/
    memcpy(loc, &node->nofentries, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    memcpy(loc, &node->type, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    if (node->type == HILBERT_LEAF_NODE) {
        for (i = 0; i < node->nofentries; i++) {
            memcpy(loc, &(node->entries.leaf[i]->pointer), sizeof (uint32_t));
            loc += sizeof (uint32_t);

            memcpy(loc, node->entries.leaf[i]->bbox, sizeof (BBox));
            loc += sizeof (BBox);
        }
    } else {
        for (i = 0; i < node->nofentries; i++) {
            memcpy(loc, &(node->entries.internal[i]->pointer), sizeof (uint32_t));
            loc += sizeof (uint32_t);

            memcpy(loc, &(node->entries.internal[i]->lhv), sizeof (hilbert_value_t));
            loc += sizeof (hilbert_value_t);

            memcpy(loc, node->entries.internal[i]->bbox, sizeof (BBox));
            loc += sizeof (BBox);
        }
    }

    //we store the node
    storage_write_one_page(si, buf, page_num, height);

    if (si->gp->io_access == DIRECT_ACCESS) {
        free(buf);
    } else {
        lwfree(buf);
    }
}

/* delete a node from file */
void del_hilbertnode(const SpatialIndex *si, int page_num, int height) {
    uint8_t *loc;
    uint8_t *buf;
    int inv = -1;

    if (si->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, si->gp->page_size, si->gp->page_size)) {
            _DEBUG(ERROR, "Allocation failed at del_hilbertnode");
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

/* serialize a node*/
void hilbertnode_serialize(const HilbertRNode *node, uint8_t *buf) {
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
        memcpy(loc, &node->nofentries, sizeof (uint32_t));
        loc += sizeof (uint32_t);

        memcpy(loc, &node->type, sizeof (uint8_t));
        loc += sizeof (uint8_t);

        if (node->type == HILBERT_LEAF_NODE) {
            for (i = 0; i < node->nofentries; i++) {
                memcpy(loc, &(node->entries.leaf[i]->pointer), sizeof (uint32_t));
                loc += sizeof (uint32_t);

                memcpy(loc, node->entries.leaf[i]->bbox, sizeof (BBox));
                loc += sizeof (BBox);
            }
        } else {
            for (i = 0; i < node->nofentries; i++) {
                memcpy(loc, &(node->entries.internal[i]->pointer), sizeof (uint32_t));
                loc += sizeof (uint32_t);

                memcpy(loc, &(node->entries.internal[i]->lhv), sizeof (hilbert_value_t));
                loc += sizeof (hilbert_value_t);

                memcpy(loc, node->entries.internal[i]->bbox, sizeof (BBox));
                loc += sizeof (BBox);
            }
        }
    }
}

/*set the coordinates of a bbox by considering a set of entries (union of all these entries)
 it also returns its LHV, largest hilbert value*/
hilbert_value_t hilbertnode_compute_bbox(const HilbertRNode *node, int srid, BBox *un) {
    BBoxCenter *center;
    hilbert_value_t ret = 1;
    int i;
    int j;

    if (node->nofentries == 0)
        _DEBUG(ERROR, "There is no entry in the current node in hilbertnode_compute_bbox");

    if (un == NULL)
        _DEBUG(ERROR, "There is no bbox (bbox is null) in hilbertnode_compute_bbox");

    if (node->type == HILBERT_LEAF_NODE) {
        rentry_create_bbox((const REntry**) node->entries.leaf, node->nofentries, un);
        //now we calculate the center of these rectangles
        center = bbox_get_center(node->entries.leaf[node->nofentries - 1]->bbox);

        //we now calculate the largest hilbert value
        //since the entries are sorted according to the lhv, we get the hilbert value from the last element
        ret = calculate_hilbert_value(center->center[0], center->center[1], srid);
    } else {
        for (i = 0; i <= MAX_DIM; i++) {
            un->max[i] = node->entries.internal[0]->bbox->max[i];
            un->min[i] = node->entries.internal[0]->bbox->min[i];
        }

        ret = node->entries.internal[node->nofentries - 1]->lhv;

        for (j = 1; j < node->nofentries; j++) {
            for (i = 0; i <= MAX_DIM; i++) {
                un->max[i] = DB_MAX(un->max[i], node->entries.internal[j]->bbox->max[i]);
                un->min[i] = DB_MIN(un->min[i], node->entries.internal[j]->bbox->min[i]);
            }
        }
    }

    return ret;
}

hilbert_value_t hilbertvalue_compute(const BBox *bbox, int srid) {
    BBoxCenter *center;
    hilbert_value_t ret = 1;

    if (bbox == NULL)
        _DEBUG(ERROR, "There is no bbox (bbox is null) in hilbertvalue_compute");

    center = bbox_get_center(bbox);

    ret = calculate_hilbert_value(center->center[0], center->center[1], srid);

    lwfree(center);

    return ret;
}

//insertion sort for those cases that the entries are almost sorted

void hilbertnode_sort_entries(HilbertRNode *node, int srid) {
    int i, j;
    if (node->type == HILBERT_INTERNAL_NODE) {
        HilbertIEntry *current;

        for (i = 1; i < node->nofentries; i++) {
            current = node->entries.internal[i];
            for (j = i - 1; j >= 0; j--) {
                if (current->lhv < node->entries.internal[j]->lhv)
                    node->entries.internal[j + 1] = node->entries.internal[j];
                else
                    break;
            }
            node->entries.internal[j + 1] = current;
        }
    } else {
        REntry *current;

        for (i = 1; i < node->nofentries; i++) {
            current = node->entries.leaf[i];
            for (j = i - 1; j >= 0; j--) {
                if (hilbertvalue_compute(current->bbox, srid) <
                        hilbertvalue_compute(node->entries.leaf[j]->bbox, srid))
                    node->entries.leaf[j + 1] = node->entries.leaf[j];
                else
                    break;
            }
            node->entries.leaf[j + 1] = current;
        }
    }
}

/*compute the dead space area of a hilbertnode */
double hilbertnode_dead_space_area(const HilbertRNode *node, int srid) {
    BBox* bbox;
    double deadspace = 0.0;
    LWGEOM *aux;
    GEOSGeometry *g;
    GEOSGeometry *un;
    GEOSGeometry *temp;
    int i;

    initGEOS(lwnotice, lwgeom_geos_error);

    if (node->nofentries >= 2) {
        if (node->type == HILBERT_LEAF_NODE)
            aux = bbox_to_geom(node->entries.leaf[0]->bbox);
        else
            aux = bbox_to_geom(node->entries.internal[0]->bbox);
        un = LWGEOM2GEOS(aux, 0);
        lwgeom_free(aux);

        for (i = 1; i < node->nofentries; i++) {
            if (node->type == HILBERT_LEAF_NODE)
                aux = bbox_to_geom(node->entries.leaf[i]->bbox);
            else
                aux = bbox_to_geom(node->entries.internal[i]->bbox);
            g = LWGEOM2GEOS(aux, 0);
            lwgeom_free(aux);

            temp = GEOSUnion(un, g);
            if (temp == NULL) { 
                //the union may fail most probably because of double precision...
                GEOSGeom_destroy(g);                
            } else {
                GEOSGeom_destroy(un);
                GEOSGeom_destroy(g);
                un = temp;
                temp = NULL;
            }
        }

        bbox = bbox_create();
        hilbertnode_compute_bbox(node, srid, bbox);
        aux = bbox_to_geom(bbox);
        g = LWGEOM2GEOS(aux, 0);
        lwgeom_free(aux);
        lwfree(bbox);

        temp = GEOSDifference(g, un);
        if (temp == NULL) //this indicates that g is a collection of points
            GEOSArea(g, &deadspace);
        else
            GEOSArea(temp, &deadspace);

        if (temp != NULL)
            GEOSGeom_destroy(temp);
        GEOSGeom_destroy(g);
        GEOSGeom_destroy(un);
    }
    return deadspace;
}

/* compute the overlapping area of a hilbertnode */
extern double hilbertnode_overlapping_area(const HilbertRNode *node) {
    int i;
    int j;
    double ovp_area = 0.0;

    if (node->type == HILBERT_LEAF_NODE) {
        for (i = 0; i < node->nofentries; i++) {
            for (j = 0; j < node->nofentries; j++) {
                if (i != j) {
                    if (bbox_check_predicate(node->entries.leaf[i]->bbox, node->entries.leaf[j]->bbox, INTERSECTS))
                        ovp_area += bbox_overlap_area(node->entries.leaf[i]->bbox, node->entries.leaf[j]->bbox);
                }
            }
        }
    } else {
        for (i = 0; i < node->nofentries; i++) {
            for (j = 0; j < node->nofentries; j++) {
                if (i != j) {
                    if (bbox_check_predicate(node->entries.internal[i]->bbox, node->entries.internal[j]->bbox, INTERSECTS))
                        ovp_area += bbox_overlap_area(node->entries.internal[i]->bbox, node->entries.internal[j]->bbox);
                }
            }
        }
    }
    return ovp_area;
}

/* compute the overlapping area of a set of HilbertIEntry entries */
double hilbertientries_overlapping_area(const HilbertIEntry **entries, int n) {
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

/*it shows in the standard output of the postgresql a hilbert node - only for debug modes*/
void hilbertnode_print(const HilbertRNode *node, int node_id) {
    int i;
    char *print;
    stringbuffer_t *sb;
    hilbert_value_t h;

    sb = stringbuffer_create();
    stringbuffer_append(sb, "HILBERTNODE(");
    stringbuffer_append(sb, "number of elements = ");
    stringbuffer_aprintf(sb, "%d, and size is %d bytes, and its type is %d => ( ",
            node->nofentries, hilbertnode_size(node), node->type);
    for (i = 0; i < node->nofentries; i++) {
        if (node->type == HILBERT_LEAF_NODE) {
            h = hilbertvalue_compute(node->entries.leaf[i]->bbox, 3857);
            stringbuffer_aprintf(sb, "(pointer %d - lhv %llu - bbox min/max %f, %f, %f, %f)  ",
                    node->entries.leaf[i]->pointer,
                    h,
                    node->entries.leaf[i]->bbox->min[0],
                    node->entries.leaf[i]->bbox->min[1],
                    node->entries.leaf[i]->bbox->max[0],
                    node->entries.leaf[i]->bbox->max[1]);
        } else {
            stringbuffer_aprintf(sb, "(pointer %d - lhv %llu - bbox min/max %f, %f, %f, %f)  ",
                    node->entries.internal[i]->pointer,
                    node->entries.internal[i]->lhv,
                    node->entries.internal[i]->bbox->min[0],
                    node->entries.internal[i]->bbox->min[1],
                    node->entries.internal[i]->bbox->max[0],
                    node->entries.internal[i]->bbox->max[1]);
        }
    }
    stringbuffer_append(sb, ")");
    print = stringbuffer_getstringcopy(sb);
    stringbuffer_destroy(sb);

    _DEBUGF(NOTICE, "NODE_ID: %d, CONTENT: %s", node_id, print);

    lwfree(print);
}

