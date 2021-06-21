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

#include "postgres.h"
#include "executor/spi.h" //execute queries to postgres
#include "utils/memutils.h"
#include <stringbuffer.h> //to handle strings in C
#include <lwgeom_functions_analytic.h> //for point_in_polygon
#include <lwgeom_geos.h> //for GEOS processing
#include <lwgeom_log.h> //because of lwnotice (for GEOS)
#include <lwgeom_pg.h>
#include <liblwgeom.h>
#include <liblwgeom.h> //because of text2cstring

#include "../main/log_messages.h"
#include "../main/statistical_processing.h" /* to collect statistical data */
#include "executor/executor.h"
#include "access/htup_details.h"
#include "query.h"
#include "../festival_config.h"

#define OFFSET_QUERY 100000

/* these functions get all the geometries from a table stored in the postgres */
/* Default filter and refinement processors
 * _ss means: spatial selection - a group of queries like range queries, point queries, and so on */
static SpatialIndexResult * default_filter_step_ss(SpatialIndex *si, LWGEOM *input,
        uint8_t predicate, uint8_t query_type);
static QueryResult * default_refinement_step_ss(SpatialIndexResult *candidates, Source *src,
        GenericParameters *gp, LWGEOM *input, uint8_t predicate);
/*this is the current method to execute the filter step (it can be changed by the user)*/
filter_step_processor_ss filter_step_ss = default_filter_step_ss;
refinement_step_processor_ss refinement_step_ss = default_refinement_step_ss;

/*we can change the current filter step and refinement step procedure only with functions different from NULL*/
void query_set_processor_ss(filter_step_processor_ss f, refinement_step_processor_ss r) {
    if (f) filter_step_ss = f;
    if (r) refinement_step_ss = r;
}

/* auxiliary functions
 */
static LWGEOM **retrieve_geoms_from_postgres(const Source *src, int *row_ids, int count);
/*when the predicate is disjoint, we have to process the complement of the obtained result!*/
static QueryResult *process_disjoint(const QueryResult *res, const char *table, const char *column, const char *pk);
/* this function checks the topological predicate by using the GEOS */
static int process_predicate(LWGEOM *input, LWGEOM *geom, uint8_t p, uint8_t refin);

SpatialIndexResult * default_filter_step_ss(SpatialIndex *si, LWGEOM *input, uint8_t p, uint8_t query_type) {
    SpatialIndexResult *result;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
    _query_predicate = p;
#endif

    /*
     ** See if we have a bounding box, add one if we don't have one.
     */
    if ((!input->bbox) && (!lwgeom_is_empty(input))) {
        lwgeom_add_bbox(input);
    }

    result = NULL;

    if (p == OVERLAP
            || p == MEET
            || p == DISJOINT /* note that we will make the complement after */
            || p == INTERSECTS)
        result = spatialindex_spatial_selection(si, input, INTERSECTS);
    else if (p == INSIDE || p == COVEREDBY)
        result = spatialindex_spatial_selection(si, input, INSIDE_OR_COVEREDBY);
    else if (p == CONTAINS)
        result = spatialindex_spatial_selection(si, input, CONTAINS);
    else if (p == COVERS)
        result = spatialindex_spatial_selection(si, input, COVERS);
    else if (p == EQUAL)
        result = spatialindex_spatial_selection(si, input, EQUAL);
    else {
        _DEBUGF(ERROR, "This is not a valid predicate: %d", p);
        return NULL;
    }

    if (result != NULL) {
        /* lets check if the filter already can correctly answer the query */
        if (query_type == RANGE_QUERY_TYPE && (p == CONTAINS || p == COVERS)) {
            result->final_result = true;
        }
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _index_cpu_time += get_elapsed_time(cpustart, cpuend);
    _filter_cpu_time += get_elapsed_time(cpustart, cpuend);

    _index_time += get_elapsed_time(start, end);
    _filter_time += get_elapsed_time(start, end);

    //this is the number of candidates
    if (result != NULL)
        _cand_num = result->num_entries;
    else
        _cand_num = 0;
#endif
    return result;
}

QueryResult * default_refinement_step_ss(SpatialIndexResult *candidates, Source *src,
        GenericParameters *gp, LWGEOM *input, uint8_t p) {
    int i;
    QueryResult *result = NULL;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    if (candidates->num_entries == 0) {
        result = create_empty_query_result();
        return result;
    } else {
        result = create_query_result(candidates->num_entries);
    }

    if (candidates == NULL) {
        _DEBUG(ERROR, "List of candidates is NULL in the refinement step");
        return NULL;
    } else {
        LWGEOM **geoms;
        int offset = 0, total = 0;

        //if these candidates already are the final result.. then 
        //we don't need to evaluate the 9-intersection matrix
        if (candidates->final_result) {
            //here we can get all the candidates from the relational table    
            geoms = retrieve_geoms_from_postgres(src, candidates->row_id, candidates->num_entries);
            for (i = 0; i < candidates->num_entries; i++) {
                result->nofentries++;
                result->geoms[result->nofentries - 1] = geoms[i];
                result->row_id[result->nofentries - 1] = candidates->row_id[i];
            }
            lwfree(geoms);
        } else {
            int j;
            int aux = candidates->num_entries;

            for (i = 0; i < candidates->num_entries; i += OFFSET_QUERY) {
                aux -= OFFSET_QUERY;

                if (aux < 0)
                    total = candidates->num_entries - offset;
                else
                    total = OFFSET_QUERY;

                geoms = retrieve_geoms_from_postgres(src, candidates->row_id + offset, total);

                for (j = 0; j < total; j++) {
                    /*check the predicate: is the input (which can be a range query) 
                     * topologically related to the current candidate by considering the predicate p? */
                    lwgeom_set_srid(input, lwgeom_get_srid(geoms[j]));
                    if (process_predicate(input, geoms[j], p, gp->refinement_type)) {
                        /*if so, we add this geom object in the final result */
                        result->nofentries++;
                        result->geoms[result->nofentries - 1] = geoms[j];
                        result->row_id[result->nofentries - 1] = candidates->row_id[i + j];
                    } else {
                        /*otherwise, we free it */
                        lwgeom_free(geoms[j]);
                    }
                }

                lwfree(geoms);

                offset += OFFSET_QUERY;
            }
        }

        /*if the predicate is disjoint then we have to perform the complement of the result
     since we had considered the intersects as predicate (see filter step implementation) */
        if (p == DISJOINT) {
            QueryResult *temp;
            temp = process_disjoint(result, src->table, src->column, src->pk);
            /* clearing the old result of memory */
            query_result_free(result, FILTER_AND_REFINEMENT_STEPS);
            /*set the temp as the final result*/
            result = temp;
        }

    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _refinement_cpu_time += get_elapsed_time(cpustart, cpuend);
    _refinement_time += get_elapsed_time(start, end);
    //this is the number of results
    _result_num = result->nofentries;
#endif

    return result;
}

void query_result_free(QueryResult * qr, uint8_t tp) {
    int i;
    if (qr->geoms != NULL && qr->max > 0) {
        if (tp == FILTER_AND_REFINEMENT_STEPS) {
            for (i = 0; i < qr->nofentries; i++) {
                if (qr->geoms[i])
                    lwgeom_free(qr->geoms[i]);
            }
        }
        lwfree(qr->geoms);
    }
    if (qr->row_id != NULL && qr->max > 0) {
        lwfree(qr->row_id);
    }
    lwfree(qr);
}

QueryResult *create_empty_query_result() {
    QueryResult *qr = (QueryResult*) lwalloc(sizeof (QueryResult));
    qr->max = 0;
    qr->geoms = NULL;
    qr->nofentries = 0;
    qr->row_id = NULL;
    return qr;
}

QueryResult *create_query_result(int max_elements) {
    QueryResult *qr = (QueryResult*) lwalloc(sizeof (QueryResult));
    qr->max = max_elements;
    qr->geoms = (LWGEOM**) lwalloc(sizeof (LWGEOM*) * qr->max);
    qr->nofentries = 0;
    qr->row_id = (int*) lwalloc(sizeof (int)*qr->max);
    return qr;
}

QueryResult * process_disjoint(const QueryResult *res, const char *table,
        const char *column, const char *pk) {
    char *query;
    stringbuffer_t *sb;
    int err;
    QueryResult *result;
    int i;
    int n;
    LWGEOM *lwgeom;
    int srid;
    char *wkt;
    int id;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    sb = stringbuffer_create();

    stringbuffer_aprintf(sb, "SELECT st_astext(%s) as geom, st_srid(%s) as srid, %s "
            "FROM %s "
            "WHERE %s NOT IN (", column, column, pk, table, pk);
    for (i = 0; i < res->nofentries; i++) {
        if (i > 0)
            stringbuffer_append(sb, ", ");
        stringbuffer_aprintf(sb, "%d", res->row_id[i]);
    }
    stringbuffer_append(sb, ");");

    query = stringbuffer_getstringcopy(sb);
    stringbuffer_destroy(sb);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "process_disjoint: could not connect to SPI manager");
        return NULL;
    }
    err = SPI_execute(query, true, 0);

    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "process_disjoint: could not execute the SELECT command");
        return NULL;
    }
    n = SPI_processed;
    result = create_query_result(n + 1);

    if (n > 0) {
        for (i = 0; i < n; i++) {
            wkt = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1);
            srid = atoi(SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 2));
            id = atoi(SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 3));

            if (wkt == NULL) {
                SPI_finish();
                _DEBUG(ERROR, "the WKT returned null")
                return NULL;
            }

            lwgeom = lwgeom_from_wkt(wkt, LW_PARSER_CHECK_NONE);
            lwgeom_set_srid(lwgeom, srid);

            result->nofentries++;
            result->geoms[result->nofentries - 1] = lwgeom;
            result->row_id[result->nofentries - 1] = id;
        }
    }
    lwfree(query);

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _retrieving_objects_cpu_time += get_elapsed_time(cpustart, cpuend);
    _retrieving_objects_time += get_elapsed_time(start, end);
#endif

    SPI_finish();

    return result;
}

LWGEOM **retrieve_geoms_from_postgres(const Source *src, int *row_ids, int count) {
    const char *query;
    stringbuffer_t *sb;
    int err;
    int i;
    bytea *bytea_ewkb;
    uint8_t *ewkb;
    LWGEOM *lwgeom = NULL;
    LWGEOM **geoms;
    MemoryContext old_context;
    char isnull;
    char *temp;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    sb = stringbuffer_create();

    stringbuffer_aprintf(sb, "SELECT ST_AsEWKB(%s) as ewkb, %s "
            "FROM %s.%s WHERE %s IN (", src->column, src->pk, src->schema, src->table, src->pk);

    for (i = 0; i < count; i++) {
        if (i > 0)
            stringbuffer_append(sb, ", ");

        stringbuffer_aprintf(sb, "%d", row_ids[i]);
    }

    stringbuffer_append(sb, ");");

    query = stringbuffer_getstring(sb);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "retrieve_geoms_from_postgres: could not connect to SPI manager");
        return NULL;
    }
    err = SPI_execute(query, true, 0);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "retrieve_geoms_from_postgres: could not execute the EXECUTE command");
        return NULL;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        _DEBUG(ERROR, "retrieve_geoms_from_postgres: returned 0 tuples")
        return NULL;
    }

    /* get the variables in upper memory context (outside of SPI) */
    /* TODO: use a narrower context to switch to */
    old_context = MemoryContextSwitchTo(TopMemoryContext);

    geoms = (LWGEOM**) lwalloc(sizeof (LWGEOM*) * count);

    for (i = 0; i < count; i++) {

#if FESTIVAL_PGSQL_VERSION == 95

        bytea_ewkb = DatumGetByteaP(SPI_getbinval(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1, &isnull));

#elif FESTIVAL_PGSQL_VERSION >= 120

        bytea_ewkb = DatumGetByteaP(SPI_getbinval(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1, (bool*) &isnull));

#endif
        ewkb = (uint8_t*) VARDATA(bytea_ewkb);

        lwgeom = lwgeom_from_wkb(ewkb, VARSIZE(bytea_ewkb) - VARHDRSZ, LW_PARSER_CHECK_NONE);

        geoms[i] = lwgeom;
        lwgeom = NULL;

        //we also modify the row_ids because the SQL query may change the order or the IDs!
        temp = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 2);
        row_ids[i] = atoi(temp);
        pfree(temp);
    }

    MemoryContextSwitchTo(old_context);

    SPI_finish();

    stringbuffer_destroy(sb);

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _retrieving_objects_cpu_time += get_elapsed_time(cpustart, cpuend);
    _retrieving_objects_time += get_elapsed_time(start, end);
#endif

    return geoms;
}

/*The meaning of this function is: input p geom
 we inverse this meaning if p is equal to CONTAINS or COVERS
 if it is equal to CONTAINS, then it will be: geom INSIDE input
 otherwise then it will be: geom COVEREDBY input*/
int process_predicate(LWGEOM *input, LWGEOM *geom, uint8_t p, uint8_t refin) {
    uint8_t type1;
    uint8_t type2;
    uint8_t pred;
    LWGEOM *geom1;
    LWGEOM *geom2;
    int result = 0;

    bool done = false;

    GEOSGeometry *g1;
    GEOSGeometry *g2;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif
    if (p == CONTAINS) {
        //then, geom inside input
        geom1 = geom;
        geom2 = input;
        pred = INSIDE;
    } else if (p == COVERS) {
        //then, geom covered by input
        geom1 = geom;
        geom2 = input;
        pred = COVEREDBY;
    } else if (p == DISJOINT) {
        //note that we will make the complement after
        //we considered the INTERSECTS here since we had considered this predicate in the filtering step!
        geom1 = input;
        geom2 = geom;
        pred = INTERSECTS;
    } else {
        geom1 = input;
        geom2 = geom;
        pred = p;
    }

    type1 = geom1->type;
    type2 = geom2->type;

    /* there are also other short-circuits for point queries
     TODO implement them later*/
    
    //_DEBUGF(NOTICE, "TYPES OF THE GEOMETRIES %d, and %d", type1, type2);

    if (refin == GEOS_AND_POINT_POLYGON) {
        //we can evaluate some short-circuits provided by the postgis here    
        if (pred == INTERSECTS && ((type1 == POINTTYPE && (type2 == POLYGONTYPE || type2 == MULTIPOLYGONTYPE)) ||
                (type2 == POINTTYPE && (type1 == POLYGONTYPE || type1 == MULTIPOLYGONTYPE)))) {
            //this a point query or range/object query with a point object
            LWPOINT *point;
            LWGEOM *lwgeom;
            uint8_t polytype;

            if (type1 == POINTTYPE) {
                point = lwgeom_as_lwpoint(geom1);
                lwgeom = geom2;
                polytype = type2;
            } else {
                point = lwgeom_as_lwpoint(geom2);
                lwgeom = geom1;
                polytype = type1;
            }

            if (polytype == POLYGONTYPE) {
                result = point_in_polygon((LWPOLY*) lwgeom, point);
            } else if (polytype == MULTIPOLYGONTYPE) {
                result = point_in_multipolygon((LWMPOLY*) lwgeom, point);
            }
            done = true;
        } else if ((pred == INSIDE || pred == COVEREDBY) &&
                (type1 == POINTTYPE && (type2 == POLYGONTYPE || type2 == MULTIPOLYGONTYPE))) {
            //this a point query or object query with a point object as input
            LWPOINT *point;
            point = lwgeom_as_lwpoint(geom1);
            if (type2 == POLYGONTYPE) {
                result = point_in_polygon((LWPOLY*) geom2, point);
            } else if (type2 == MULTIPOLYGONTYPE) {
                result = point_in_multipolygon((LWMPOLY*) geom2, point);
            }
            done = true;
        } else if ((pred == INSIDE || pred == COVEREDBY) &&
                (type2 == POINTTYPE && (type1 == POLYGONTYPE || type1 == MULTIPOLYGONTYPE))) {
            //this a range/object query on a spatial database with points        
            LWPOINT *point;
            point = lwgeom_as_lwpoint(geom2);
            if (type1 == POLYGONTYPE) {
                result = point_in_polygon((LWPOLY*) geom1, point);
            } else if (type1 == MULTIPOLYGONTYPE) {
                result = point_in_multipolygon((LWMPOLY*) geom1, point);
            }
            done = true;
        }

        if (result != -1) /* not outside */ {
            result = 1;
        } else {
            result = 0;
        }
    }

    if (!done && (refin == ONLY_GEOS || refin == GEOS_AND_POINT_POLYGON)) {
        initGEOS(lwnotice, lwgeom_geos_error);
        g1 = LWGEOM2GEOS(geom1, 0);
        g2 = LWGEOM2GEOS(geom2, 0);
        switch (p) {
            case INTERSECTS:
            case DISJOINT: //we make the complement of this predicate after
                result = GEOSIntersects(g1, g2);
                break;
            case OVERLAP:
                result = GEOSOverlaps(g1, g2);
                break;
            case EQUAL:
                result = GEOSEquals(g1, g2);
                break;
            case INSIDE:
                //our inside follows the definition of many papers 
                //(other papers may refer to this operation as ContainsProperly)
                result = GEOSRelatePattern(g2, g1, "T**FF*FF*");
                break;
            case MEET:
                result = GEOSTouches(g1, g2);
                break;
            case COVEREDBY:
                result = GEOSCoveredBy(g1, g2);
                break;
            default:
                _DEBUGF(ERROR, "Predicate %d invalid", p);
        }
        GEOSGeom_destroy(g1);
        GEOSGeom_destroy(g2);
        /*GEOS returned an error to evaluate the predicate*/
        if (result == 2) {
            _DEBUGF(ERROR, "GEOS is not able to compute the predicate %d", p);
        }
    } else {
        if (!done) {
            _DEBUGF(ERROR, "There is no this refinement type: %d", refin);
        }
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _processing_predicates_cpu_time += get_elapsed_time(cpustart, cpuend);
    _processing_predicates_time += get_elapsed_time(start, end);
#endif

    return result;
}

/*this function is responsible to handle several types of queries, such as:
 (i) range query considering input rectangular-shaped,
 (ii) generic object as input, named object query,
 (iii) point query, which considers the input as a point object
 * TO include other types of queries.
 * processing_type refers to the execution of (i) only filter step, (ii) or filter plus refinement step.
 */
QueryResult *process_spatial_selection(SpatialIndex *si, LWGEOM *input,
        uint8_t predicate, uint8_t query_type, uint8_t processing_type) {
    SpatialIndexResult *sir;
    QueryResult *result = NULL;

    if (processing_type == FILTER_AND_REFINEMENT_STEPS) {
        /* execution of the filter step*/
        sir = filter_step_ss(si, input, predicate, query_type);
        /* execution of the refinement step*/
        result = refinement_step_ss(sir, si->src, si->gp, input, predicate);

        spatial_index_result_free(sir);
    } else if (processing_type == ONLY_FILTER_STEP) {
        /* execution of the filter step*/
        sir = filter_step_ss(si, input, predicate, query_type);
        /* now we transform sir into our result
         * in this case the results will contain the row_id of the candidates only
         * TODO - include also the MBR of the entries here */
        if (sir->num_entries == 0) {
            result = create_empty_query_result();
        } else {
            result = create_query_result(sir->num_entries);
            memcpy(result->row_id, sir->row_id, sir->num_entries * sizeof (int));
            result->nofentries = sir->num_entries;
        }
        
        spatial_index_result_free(sir);
    } else {
        _DEBUGF(ERROR, "Invalid parameter value for processing_option (%d)", processing_type);
    }

    return result;
}
