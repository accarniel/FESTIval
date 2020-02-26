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
#include "../main/header_handler.h" //for header storage
#include "../main/statistical_processing.h"

#include "efind_buffer_manager.h"
#include "efind_page_handler_augmented.h"

/*********************************
 * functions in order to make FASINF indices following the interface SpatialIndex (see spatial_index.h)
 *********************************/

static uint8_t efindindex_get_type(const SpatialIndex *si) {
    eFINDIndex *fi = (void *) si;
    return fi->efind_type_index;
}

static bool efindindex_insert(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    eFINDIndex *fi = (void *) si;
    if (fi->efind_type_index == eFIND_RTREE_TYPE) {
        eFINDRTree *fr;
        fr = fi->efind_index.efind_rtree;
        rtree_set_efindspecification(fr->spec);
        spatialindex_insert(&(fr->rtree->base), pointer, geom);
    } else if (fi->efind_type_index == eFIND_RSTARTREE_TYPE) {
        eFINDRStarTree *fr;
        fr = fi->efind_index.efind_rstartree;
        rstartree_set_efindspecification(fr->spec);
        spatialindex_insert(&(fr->rstartree->base), pointer, geom);
    } else if(fi->efind_type_index == eFIND_HILBERT_RTREE_TYPE) {
        eFINDHilbertRTree *fr;
        fr = fi->efind_index.efind_hilbertrtree;
        hilbertrtree_set_efindspecification(fr->spec);
        efind_pagehandler_set_srid(fr->hilbertrtree->spec->srid);
        spatialindex_insert(&(fr->hilbertrtree->base), pointer, geom);        
    } else {
        _DEBUGF(ERROR, "Unknown eFIND index %d", fi->efind_type_index);
    }
    return true;
}

static bool efindindex_remove(SpatialIndex *si, int pointer, const LWGEOM *geom) {
    eFINDIndex *fi = (void *) si;
    if (fi->efind_type_index == eFIND_RTREE_TYPE) {
        eFINDRTree *fr;
        fr = fi->efind_index.efind_rtree;
        rtree_set_efindspecification(fr->spec);
        return spatialindex_remove(&(fr->rtree->base), pointer, geom);
    } else if (fi->efind_type_index == eFIND_RSTARTREE_TYPE) {
        eFINDRStarTree *fr;
        fr = fi->efind_index.efind_rstartree;
        rstartree_set_efindspecification(fr->spec);
        return spatialindex_remove(&(fr->rstartree->base), pointer, geom);
    } else if(fi->efind_type_index == eFIND_HILBERT_RTREE_TYPE) {
        eFINDHilbertRTree *fr;
        fr = fi->efind_index.efind_hilbertrtree;
        hilbertrtree_set_efindspecification(fr->spec);
        efind_pagehandler_set_srid(fr->hilbertrtree->spec->srid);
        return spatialindex_remove(&(fr->hilbertrtree->base), pointer, geom);        
    } else {
        _DEBUGF(ERROR, "Unknown eFIND index %d", fi->efind_type_index);
    }
}

static bool efindindex_update(SpatialIndex *si, int oldpointer, const LWGEOM *oldgeom,
        int newpointer, const LWGEOM *newgeom) {
    eFINDIndex *fi = (void *) si;
    if (fi->efind_type_index == eFIND_RTREE_TYPE) {
        eFINDRTree *fr;
        fr = fi->efind_index.efind_rtree;
        rtree_set_efindspecification(fr->spec);
        return spatialindex_update(&(fr->rtree->base), oldpointer, oldgeom, newpointer, newgeom);
    } else if (fi->efind_type_index == eFIND_RSTARTREE_TYPE) {
        eFINDRStarTree *fr;
        fr = fi->efind_index.efind_rstartree;
        rstartree_set_efindspecification(fr->spec);
        return spatialindex_update(&(fr->rstartree->base), oldpointer, oldgeom, newpointer, newgeom);
    } else if(fi->efind_type_index == eFIND_HILBERT_RTREE_TYPE) {
        eFINDHilbertRTree *fr;
        fr = fi->efind_index.efind_hilbertrtree;
        hilbertrtree_set_efindspecification(fr->spec);
        efind_pagehandler_set_srid(fr->hilbertrtree->spec->srid);
        return spatialindex_update(&(fr->hilbertrtree->base), oldpointer, oldgeom, newpointer, newgeom);        
    } else {
        _DEBUGF(ERROR, "Unknown eFIND index %d", fi->efind_type_index);
    }
}

static SpatialIndexResult *efindindex_search_ss(SpatialIndex *si, const LWGEOM *search_object, uint8_t predicate) {
    eFINDIndex *fi = (void *) si;
    if (fi->efind_type_index == eFIND_RTREE_TYPE) {
        eFINDRTree *fr;
        fr = fi->efind_index.efind_rtree;
        rtree_set_efindspecification(fr->spec);
        return spatialindex_spatial_selection(&(fr->rtree->base), search_object, predicate);
    } else if (fi->efind_type_index == eFIND_RSTARTREE_TYPE) {
        eFINDRStarTree *fr;
        fr = fi->efind_index.efind_rstartree;
        rstartree_set_efindspecification(fr->spec);
        return spatialindex_spatial_selection(&(fr->rstartree->base), search_object, predicate);
    } else if(fi->efind_type_index == eFIND_HILBERT_RTREE_TYPE) {
        eFINDHilbertRTree *fr;
        fr = fi->efind_index.efind_hilbertrtree;
        hilbertrtree_set_efindspecification(fr->spec);
        efind_pagehandler_set_srid(fr->hilbertrtree->spec->srid);
        return spatialindex_spatial_selection(&(fr->hilbertrtree->base), search_object, predicate);        
    } else {
        _DEBUGF(ERROR, "Unknown eFIND index %d", fi->efind_type_index);
    }
}

static bool efindindex_header_writer(SpatialIndex *si, const char *file) {
    eFINDIndex *fi = (void *) si;
    festival_header_writer(file, fi->efind_type_index, si);
    return true;
}

static void efindindex_destroy(SpatialIndex *si) {
    eFINDIndex *fi = (void *) si;
    if (fi->efind_type_index == eFIND_RTREE_TYPE) {
        eFINDRTree *fr;        
        fr = fi->efind_index.efind_rtree;
        lwfree(fr->spec->log_file);
        lwfree(fr->spec);
        spatialindex_destroy(&(fr->rtree->base));
    } else if (fi->efind_type_index == eFIND_RSTARTREE_TYPE) {
        eFINDRStarTree *fr;
        fr = fi->efind_index.efind_rstartree;
        lwfree(fr->spec->log_file);
        lwfree(fr->spec);
        spatialindex_destroy(&(fr->rstartree->base));
    } else if(fi->efind_type_index == eFIND_HILBERT_RTREE_TYPE) {
        eFINDHilbertRTree *fr;
        fr = fi->efind_index.efind_hilbertrtree;
        lwfree(fr->spec->log_file);
        lwfree(fr->spec);
        spatialindex_destroy(&(fr->hilbertrtree->base));     
    }  else {
        _DEBUGF(ERROR, "Unknown eFIND index %d", fi->efind_type_index);
    }
    lwfree(fi);
}

SpatialIndex *efindrtree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, eFINDSpecification *fs, bool persist) {
    eFINDIndex *fi;
    RTree *rt;
    SpatialIndex *si_rt;

    /*define the general functions of the fast*/
    static const SpatialIndexInterface vtable = {efindindex_get_type,
        efindindex_insert, efindindex_remove, efindindex_update, efindindex_search_ss,
        efindindex_header_writer, efindindex_destroy
    };
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    fi = (eFINDIndex*) lwalloc(sizeof (eFINDIndex));
    memcpy(&fi->base, &base, sizeof (base));
    fi->efind_type_index = eFIND_RTREE_TYPE;

    si_rt = rtree_empty_create(file, src, gp, bs, false);
    rt = (void *) si_rt;
    rt->type = eFIND_RTREE_TYPE;
    if (persist)
        rt->current_node = rnode_create_empty();

    fi->efind_index.efind_rtree = (eFINDRTree*) lwalloc(sizeof (eFINDRTree));
    fi->efind_index.efind_rtree->spec = fs;
    fi->efind_index.efind_rtree->rtree = rt;

    if (persist) {
        //put the empty root node in the buffer
        efind_buf_create_node(&fi->base, fs, 0, 0);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &fi->base;
}

SpatialIndex *efindrstartree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, eFINDSpecification *fs, bool persist) {
    eFINDIndex *fi;
    RStarTree *rstar;
    SpatialIndex *si_rstar;

    /*define the general functions of the fast*/
    static const SpatialIndexInterface vtable = {efindindex_get_type,
        efindindex_insert, efindindex_remove, efindindex_update, efindindex_search_ss,
        efindindex_header_writer, efindindex_destroy
    };
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    fi = (eFINDIndex*) lwalloc(sizeof (eFINDIndex));
    memcpy(&fi->base, &base, sizeof (base));
    fi->efind_type_index = eFIND_RSTARTREE_TYPE;

    si_rstar = rstartree_empty_create(file, src, gp, bs, false);
    rstar = (void *) si_rstar;
    rstar->type = eFIND_RSTARTREE_TYPE;
    if (persist)
        rstar->current_node = rnode_create_empty();

    fi->efind_index.efind_rstartree = (eFINDRStarTree*) lwalloc(sizeof (eFINDRStarTree));
    fi->efind_index.efind_rstartree->spec = fs;
    fi->efind_index.efind_rstartree->rstartree = rstar;

    if (persist) {        
        //put the empty root node in the buffer
        efind_buf_create_node(&fi->base, fs, 0, 0);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &fi->base;
}

SpatialIndex *efindhilbertrtree_empty_create(char *file, Source *src, GenericParameters *gp, 
        BufferSpecification *bs, eFINDSpecification *fs, bool persist) {
    eFINDIndex *fi;
    HilbertRTree *rt;
    SpatialIndex *si_rt;

    /*define the general functions of the fast*/
    static const SpatialIndexInterface vtable = {efindindex_get_type,
        efindindex_insert, efindindex_remove, efindindex_update, efindindex_search_ss,
        efindindex_header_writer, efindindex_destroy
    };
    static SpatialIndex base = {&vtable};
    base.bs = bs;
    base.gp = gp;
    base.src = src;
    base.index_file = file;

    fi = (eFINDIndex*) lwalloc(sizeof (eFINDIndex));
    memcpy(&fi->base, &base, sizeof (base));
    fi->efind_type_index = eFIND_HILBERT_RTREE_TYPE;

    si_rt = hilbertrtree_empty_create(file, src, gp, bs, false);
    rt = (void *) si_rt;
    rt->type = eFIND_HILBERT_RTREE_TYPE;
    if (persist)
        rt->current_node = hilbertnode_create_empty(HILBERT_LEAF_NODE);

    fi->efind_index.efind_hilbertrtree = (eFINDHilbertRTree*) lwalloc(sizeof (eFINDHilbertRTree));
    fi->efind_index.efind_hilbertrtree->spec = fs;
    fi->efind_index.efind_hilbertrtree->hilbertrtree = rt;

    if (persist) {
        //put the empty root node in the buffer
        efind_buf_create_node(&fi->base, fs, 0, 0);

#ifdef COLLECT_STATISTICAL_DATA
        _written_leaf_node_num++;
        insert_writes_per_height(0, 1);
#endif
    }

    return &fi->base;
}
