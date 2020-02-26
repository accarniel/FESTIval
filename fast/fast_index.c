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

#include "../main/log_messages.h"
#include "fast_index.h"
#include "fast_buffer.h"
#include "../main/header_handler.h" //for header storage
#include "../main/statistical_processing.h"

/*********************************
 * functions in order to make FAST-based indices a standard SpatialIndex (see spatial_index.h)
 *********************************/

static uint8_t fastindex_get_type(const SpatialIndex *si) {
    FASTIndex *fi = (void *) si;
    return fi->fast_type_index;
}

static bool fastindex_insert(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    FASTIndex *fi = (void *) si;
    if (fi->fast_type_index == FAST_RTREE_TYPE) {
        FASTRTree *fr;
        fr = fi->fast_index.fast_rtree;
        rtree_set_fastspecification(fr->spec);
        spatialindex_insert(&(fr->rtree->base), pointer, geom);
    } else if (fi->fast_type_index == FAST_RSTARTREE_TYPE) {
        FASTRStarTree *fr;
        fr = fi->fast_index.fast_rstartree;
        rstartree_set_fastspecification(fr->spec);
        spatialindex_insert(&(fr->rstartree->base), pointer, geom);
    } else if (fi->fast_type_index == FAST_HILBERT_RTREE_TYPE) {
        FASTHilbertRTree *fr;
        fr = fi->fast_index.fast_hilbertrtree;
        hilbertrtree_set_fastspecification(fr->spec);
        spatialindex_insert(&(fr->hilbertrtree->base), pointer, geom);
    } else {
        _DEBUGF(ERROR, "Unknown fast index %d", fi->fast_type_index);
    }
    return true;
}

static bool fastindex_remove(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    FASTIndex *fi = (void *) si;
    if (fi->fast_type_index == FAST_RTREE_TYPE) {
        FASTRTree *fr;
        fr = fi->fast_index.fast_rtree;
        rtree_set_fastspecification(fr->spec);
        return spatialindex_remove(&(fr->rtree->base), pointer, geom);
    } else if (fi->fast_type_index == FAST_RSTARTREE_TYPE) {
        FASTRStarTree *fr;
        fr = fi->fast_index.fast_rstartree;
        rstartree_set_fastspecification(fr->spec);
        return spatialindex_remove(&(fr->rstartree->base), pointer, geom);
    } else if (fi->fast_type_index == FAST_HILBERT_RTREE_TYPE) {
        FASTHilbertRTree *fr;
        fr = fi->fast_index.fast_hilbertrtree;
        hilbertrtree_set_fastspecification(fr->spec);
        return spatialindex_remove(&(fr->hilbertrtree->base), pointer, geom);
    } else {
        _DEBUGF(ERROR, "Unknown fast index %d", fi->fast_type_index);
    }
}

static bool fastindex_update(SpatialIndex *si, int oldpointer, const LWGEOM *oldgeom,
        int newpointer, const LWGEOM *newgeom) {
    FASTIndex *fi = (void *) si;
    if (fi->fast_type_index == FAST_RTREE_TYPE) {
        FASTRTree *fr;
        fr = fi->fast_index.fast_rtree;
        rtree_set_fastspecification(fr->spec);
        return spatialindex_update(&(fr->rtree->base), oldpointer, oldgeom, newpointer, newgeom);
    } else if (fi->fast_type_index == FAST_RSTARTREE_TYPE) {
        FASTRStarTree *fr;
        fr = fi->fast_index.fast_rstartree;
        rstartree_set_fastspecification(fr->spec);
        return spatialindex_update(&(fr->rstartree->base), oldpointer, oldgeom, newpointer, newgeom);
    } else if (fi->fast_type_index == FAST_HILBERT_RTREE_TYPE) {
        FASTHilbertRTree *fr;
        fr = fi->fast_index.fast_hilbertrtree;
        hilbertrtree_set_fastspecification(fr->spec);
        return spatialindex_update(&(fr->hilbertrtree->base), oldpointer, oldgeom, newpointer, newgeom);
    } else {
        _DEBUGF(ERROR, "Unknown fast index %d", fi->fast_type_index);
    }
}

static SpatialIndexResult *fastindex_search_ss(SpatialIndex *si, const LWGEOM *search_object, uint8_t predicate) {
    FASTIndex *fi = (void *) si;
    if (fi->fast_type_index == FAST_RTREE_TYPE) {
        FASTRTree *fr;
        fr = fi->fast_index.fast_rtree;
        rtree_set_fastspecification(fr->spec);
        return spatialindex_spatial_selection(&(fr->rtree->base), search_object, predicate);
    } else if (fi->fast_type_index == FAST_RSTARTREE_TYPE) {
        FASTRStarTree *fr;
        fr = fi->fast_index.fast_rstartree;
        rstartree_set_fastspecification(fr->spec);
        return spatialindex_spatial_selection(&(fr->rstartree->base), search_object, predicate);
    } else if (fi->fast_type_index == FAST_HILBERT_RTREE_TYPE) {
        FASTHilbertRTree *fr;
        fr = fi->fast_index.fast_hilbertrtree;
        hilbertrtree_set_fastspecification(fr->spec);
        return spatialindex_spatial_selection(&(fr->hilbertrtree->base), search_object, predicate);
    } else {
        _DEBUGF(ERROR, "Unknown fast index %d", fi->fast_type_index);
    }
}

static bool fastindex_header_writer(SpatialIndex *si, const char *file) {
    FASTIndex *fi = (void *) si;
    festival_header_writer(file, fi->fast_type_index, si);
    return true;
}

static void fastindex_destroy(SpatialIndex *si) {
    FASTIndex *fi = (void *) si;
    if (fi->fast_type_index == FAST_RTREE_TYPE) {
        FASTRTree *fr;
        fr = fi->fast_index.fast_rtree;
        lwfree(fr->spec->log_file);
        lwfree(fr->spec);
        spatialindex_destroy(&(fr->rtree->base));
    } else if (fi->fast_type_index == FAST_RSTARTREE_TYPE) {
        FASTRStarTree *fr;
        fr = fi->fast_index.fast_rstartree;
        lwfree(fr->spec->log_file);
        lwfree(fr->spec);
        spatialindex_destroy(&(fr->rstartree->base));
    } else if(fi->fast_type_index == FAST_HILBERT_RTREE_TYPE) {
        FASTHilbertRTree *fr;
        fr = fi->fast_index.fast_hilbertrtree;
        lwfree(fr->spec->log_file);
        lwfree(fr->spec);
        spatialindex_destroy(&(fr->hilbertrtree->base));        
    } else {
        _DEBUGF(ERROR, "Unknown fast index %d", fi->fast_type_index);
    }
    lwfree(fi);
}

SpatialIndex *fastrtree_empty_create(char *file, Source *src, GenericParameters *gp,
        BufferSpecification *bs, FASTSpecification *fs, bool persist) {
    FASTIndex *fi;
    RTree *rt;
    SpatialIndex *si_rt;

    /*define the general functions of the fast*/
    static const SpatialIndexInterface vtable = {fastindex_get_type,
        fastindex_insert, fastindex_remove, fastindex_update, fastindex_search_ss,
        fastindex_header_writer, fastindex_destroy};
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    fi = (FASTIndex*) lwalloc(sizeof (FASTIndex));
    memcpy(&fi->base, &base, sizeof (base));
    fi->fast_type_index = FAST_RTREE_TYPE;

    si_rt = rtree_empty_create(file, src, gp, bs, false);
    rt = (void *) si_rt;
    rt->type = FAST_RTREE_TYPE;
    if (persist)
        rt->current_node = rnode_create_empty();

    fi->fast_index.fast_rtree = (FASTRTree*) lwalloc(sizeof (FASTRTree));
    fi->fast_index.fast_rtree->spec = fs;
    fi->fast_index.fast_rtree->rtree = rt;

    if (persist) {
        //put the empty root node in the buffer
        fb_put_new_node(&fi->base, fs, 0, (void *) rnode_create_empty(), 0);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &fi->base;
}

SpatialIndex *fastrstartree_empty_create(char *file, Source *src, GenericParameters *gp,
        BufferSpecification *bs, FASTSpecification *fs, bool persist) {
    FASTIndex *fi;
    RStarTree *rstar;
    SpatialIndex *si_rstar;

    /*define the general functions of the fast*/
    static const SpatialIndexInterface vtable = {fastindex_get_type,
        fastindex_insert, fastindex_remove, fastindex_update, fastindex_search_ss,
        fastindex_header_writer, fastindex_destroy};
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    fi = (FASTIndex*) lwalloc(sizeof (FASTIndex));
    memcpy(&fi->base, &base, sizeof (base));
    fi->fast_type_index = FAST_RSTARTREE_TYPE;

    si_rstar = rstartree_empty_create(file, src, gp, bs, false);
    rstar = (void *) si_rstar;
    rstar->type = FAST_RSTARTREE_TYPE;
    if (persist)
        rstar->current_node = rnode_create_empty();

    fi->fast_index.fast_rstartree = (FASTRStarTree*) lwalloc(sizeof (FASTRStarTree));
    fi->fast_index.fast_rstartree->spec = fs;
    fi->fast_index.fast_rstartree->rstartree = rstar;

    if (persist) {
        //put the empty root node in the buffer
        fb_put_new_node(&fi->base, fs, 0, (void *) rnode_create_empty(), 0);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &fi->base;
}

SpatialIndex *fasthilbertrtree_empty_create(char *file, Source *src, GenericParameters *gp,
        BufferSpecification *bs, FASTSpecification *fs, bool persist) {
    FASTIndex *fi;
    HilbertRTree *hrt;
    SpatialIndex *si_hrt;

    /*define the general functions of the fast*/
    static const SpatialIndexInterface vtable = {fastindex_get_type,
        fastindex_insert, fastindex_remove, fastindex_update, fastindex_search_ss,
        fastindex_header_writer, fastindex_destroy};
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    fi = (FASTIndex*) lwalloc(sizeof (FASTIndex));
    memcpy(&fi->base, &base, sizeof (base));
    fi->fast_type_index = FAST_HILBERT_RTREE_TYPE;

    si_hrt = hilbertrtree_empty_create(file, src, gp, bs, false);
    hrt = (void *) si_hrt;
    hrt->type = FAST_HILBERT_RTREE_TYPE;
    if (persist) 
        hrt->current_node = hilbertnode_create_empty(HILBERT_LEAF_NODE);

    fi->fast_index.fast_hilbertrtree = (FASTHilbertRTree*) lwalloc(sizeof (FASTHilbertRTree));
    fi->fast_index.fast_hilbertrtree->spec = fs;
    fi->fast_index.fast_hilbertrtree->hilbertrtree = hrt;

    if (persist) {
        //put the empty root node in the buffer
        fb_put_new_node(&fi->base, fs, 0, (void *) hilbertnode_create_empty(HILBERT_LEAF_NODE), 0);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &fi->base;
}
