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

#include <liblwgeom.h> //for lwalloc
#include <string.h> //for mmmove
#include "festival_defs.h"
#include "spatial_index.h"
#include "header_handler.h"
#include "log_messages.h"

/*this is the current method to read a spatial index from its header file*/
construct_from_header constructor = festival_get_spatialindex;

/*we are able to modify it is we want*/
void index_specification_set_constructor(construct_from_header cons) {
    if (cons) constructor = cons;
}

SpatialIndex *spatialindex_from_header(const char *file) {
    return constructor(file);
}

SpatialIndexResult *spatial_index_result_create() {
    SpatialIndexResult *sir = (SpatialIndexResult*) lwalloc(sizeof (SpatialIndexResult));
    sir->max = 2;
    sir->num_entries = 0;
    sir->row_id = (int*) lwalloc(sizeof (int) * sir->max);
    sir->final_result = false; //the default is false
    return sir;
}

void spatial_index_result_free(SpatialIndexResult *sir) {
    if (sir->row_id) lwfree(sir->row_id);
    lwfree(sir);
}

void spatial_index_result_add(SpatialIndexResult *result, int row_id) {
    /* we need to realloc more space */
    if (result->max < result->num_entries + 1) {
        result->max *= 2;
        result->row_id = (int*) lwrealloc(result->row_id, sizeof (int) * result->max);
    }

    result->row_id[result->num_entries] = row_id;
    result->num_entries++;
}

Source *create_source(char *schema, char *table, char *column, char *pk) {
    Source *src = (Source*) lwalloc(sizeof (Source));
    src->schema = schema;
    src->table = table;
    src->column = column;
    src->pk = pk;
    return src;
}

void source_free(Source *src) {
    lwfree(src->schema);
    lwfree(src->table);
    lwfree(src->column);
    lwfree(src->pk);
    lwfree(src);
}

GenericParameters *generic_parameters_create(StorageSystem *ss, uint8_t io,
        int ps, uint8_t ref) {
    GenericParameters *gp = (GenericParameters*) lwalloc(sizeof (GenericParameters));
    gp->io_access = io;
    gp->page_size = ps;
    gp->refinement_type = ref;
    gp->storage_system = ss;
    return gp;
}

void generic_parameters_free(GenericParameters *gp) {
    lwfree(gp);
}

RTreesInfo *rtreesinfo_create(int rp, int h, int lap) {
    RTreesInfo *cri = (RTreesInfo*) lwalloc(sizeof (RTreesInfo));
    cri->height = h;
    cri->last_allocated_page = lap;
    cri->root_page = rp;

    cri->empty_pages = NULL;
    cri->max_empty_pages = 0;
    cri->nof_empty_pages = 0;
    return cri;
}

void rtreesinfo_free(RTreesInfo *cri) {
    if (cri->empty_pages && cri->max_empty_pages > 0) lwfree(cri->empty_pages);
    lwfree(cri);
}

void rtreesinfo_set_empty_pages(RTreesInfo *cri,
        int *empty_pages, int nof, int max) {
    if (cri->empty_pages && cri->max_empty_pages > 0) lwfree(cri->empty_pages);
    cri->empty_pages = empty_pages;
    cri->max_empty_pages = max;
    cri->nof_empty_pages = nof;
}

void rtreesinfo_add_empty_page(RTreesInfo *cri, int page) {
    /*if there is not element created here.. we alloc some initial space*/
    if (cri->empty_pages == NULL) {
        cri->max_empty_pages = 2;
        cri->nof_empty_pages = 0;
        cri->empty_pages = (int*) lwalloc(sizeof (int) * cri->max_empty_pages);
    }
    /* we need to realloc more space */
    if (cri->max_empty_pages < cri->nof_empty_pages + 1) {
        cri->max_empty_pages *= 2;
        cri->empty_pages = (int*) lwrealloc(cri->empty_pages,
                sizeof (int) * cri->max_empty_pages);
    }

    cri->empty_pages[cri->nof_empty_pages] = page;
    cri->nof_empty_pages++;
}

void rtreesinfo_remove_empty_page(RTreesInfo *cri, int position) {
    if (position < cri->nof_empty_pages && position >= 0) {
        /* If the point is any but the last, we need to copy the data back one point */
        if (position < cri->nof_empty_pages - 1) {
            memmove(cri->empty_pages + position, cri->empty_pages + (position + 1), sizeof (int) * (cri->nof_empty_pages - position - 1));
        }
        /* We have one less page */
        cri->nof_empty_pages--;
    }
}

int rtreesinfo_get_max_entries(uint8_t idx_type, int page_size, int entry_size, double perc) {
    switch (idx_type) {
        case CONVENTIONAL_RSTARTREE:
        case CONVENTIONAL_RTREE:
        case FAST_RTREE_TYPE:
        case FAST_RSTARTREE_TYPE:
        case FORTREE_TYPE:
        case eFIND_RTREE_TYPE:
        case eFIND_RSTARTREE_TYPE:
            return (int) ceil(floor((page_size - sizeof (uint32_t)) / entry_size) * perc);
        case CONVENTIONAL_HILBERT_RTREE:
        case FAST_HILBERT_RTREE_TYPE:
        case eFIND_HILBERT_RTREE_TYPE:
            //todo we are currently storing the type of the node for hilbert r-trees
            //however, I believe that this is not needed (check if it is possible to not store it)
            //note that we need to do this checking for all indexes based on the hilbert r-tree
            return (int) ceil(floor((page_size - sizeof (uint32_t) - sizeof(uint8_t)) / entry_size) * perc);
        default:
        {
            _DEBUGF(ERROR, "Index type (%d) not supported in rtreesinfo_get_max_entries", idx_type);
            return 0;
        }
    }
}

int rtreesinfo_get_min_entries(uint8_t idx_type, int max_entries, double perc) {
    switch (idx_type) {
        case CONVENTIONAL_RSTARTREE:
        case CONVENTIONAL_RTREE:
        case CONVENTIONAL_HILBERT_RTREE:
        case FAST_RTREE_TYPE:
        case FAST_RSTARTREE_TYPE:
        case FAST_HILBERT_RTREE_TYPE:
        case FORTREE_TYPE:
        case eFIND_RTREE_TYPE:
        case eFIND_RSTARTREE_TYPE:
        case eFIND_HILBERT_RTREE_TYPE:
        {
            int min = (int) ceil(max_entries * perc);
            if (min < 2)
                return 2;
            return min;
        }
        default:
        {
            _DEBUGF(ERROR, "Index type (%d) not supported in rtreesinfo_get_min_entries", idx_type);
            return 0;
        }
    }
}

int rtreesinfo_get_valid_page(RTreesInfo *info) {
    //we firstly check if there is empty pages to be reused
    if (info->empty_pages && info->nof_empty_pages > 0) {
        //if so, we get the first empty page
        int p;
        p = info->empty_pages[0];
        rtreesinfo_remove_empty_page(info, 0);
        return p;
    } else {
        //otherwise, we 'create' a new valid page
        info->last_allocated_page++;
        return info->last_allocated_page;
    }
}

bool array_contains_element(int *vec, int n, int v) {
    int i;
    for (i = 0; i < n; i++) {
        if (vec[i] == v)
            return true;
    }
    return false;
}
