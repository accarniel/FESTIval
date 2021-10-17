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

/* this the main file that define the postgresql functions */
/*needed libraries from postgres*/
#include "postgres.h"
#include "funcapi.h"
#include "fmgr.h"
#include "executor/spi.h"
#include "utils/memutils.h"

#include <math.h>
#include <liblwgeom.h>
#include <unistd.h> //to handle POSTGIS objects
#include "lwgeom_pg.h" //to convert POSTGIS objects

#include "../main/storage_handler.h" //for storage system types
#include "../main/io_handler.h" //for DIRECT or CONVENTIONAL access types
#include "../main/header_handler.h" //to get spatial index from header
#include "../main/log_messages.h" //for messages
#include "../main/statistical_processing.h" //for statistical processing

/*information about the specification of each index*/
#include "../rtree/rtree.h"
#include "../rstartree/rstartree.h"
#include "../hilbertrtree/hilbertrtree.h"
#include "../fast/fast_buffer.h"
#include "../fortree/fortree_buffer.h"
#include "../fast/fast_flush_module.h"
#include "../efind/efind_flushing_manager.h"
#include "../efind/efind_read_buffer_policies.h"

#include "query.h" //for query processing

#include "access/htup_details.h"
#include "utils/builtins.h"

#include "../festival_config.h"

/*TO-DO validate the input values in order to create spatial index with valid configurations*/

/*these functions get informations from the FESTIval data schema in order to create a new index!*/
static GenericParameters *read_basicconfiguration_from_fds(int bc_id);
static Source *read_source_from_fds(int src_id);
static BufferSpecification *read_bufferconfiguration_from_fds(int buf_id, int page_size);
static void set_rtreespec_from_fds(RTreeSpecification *spec, int sc_id, int page_size);
static void set_rstartreespec_from_fds(RStarTreeSpecification *rs, int sc_id, int page_size);
static void set_hilbertrtreespec_from_fds(HilbertRTreeSpecification *spec, int sc_id, int page_size);
static FASTSpecification *set_fastspec_from_fds(int sc_id, int *index_type);
static FORTreeSpecification *set_fortreespec_from_fds(int sc_id, int page_size);
static eFINDSpecification *set_efindspec_from_fds(int sc_id, int *index_type);

GenericParameters *read_basicconfiguration_from_fds(int bc_id) {
    GenericParameters *gp;
    char query[512];
    int err;
    char *ss;
    char *io;
    char *r;

    gp = (GenericParameters*) lwalloc(sizeof (GenericParameters));
    gp->storage_system = (StorageSystem*) lwalloc(sizeof (StorageSystem));

    sprintf(query, "SELECT page_size, ss.ss_id, upper(storage_system), upper(io_access), upper(refinement_type) "
            "FROM fds.basicconfiguration as bc, fds.storagesystem as ss WHERE bc.ss_id = ss.ss_id AND bc_id = %d;", bc_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "read_basicconfiguration_from_fds: could not connect to SPI manager");
        return NULL;
    }
    err = SPI_execute(query, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "read_basicconfiguration_from_fds: could not execute the SELECT command");
        return NULL;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        _DEBUGF(ERROR, "the bc_id (%d) does not exist in the table", bc_id);
        return NULL;
    }

    gp->page_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));
    gp->storage_system->ss_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2));
    ss = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 3);
    io = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4);
    r = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 5);
    gp->bc_id = bc_id;

    if (strcmp(ss, "FLASH SSD") == 0) {
        gp->storage_system->type = SSD;
    } else if (strcmp(ss, "HDD") == 0) {
        gp->storage_system->type = HDD;
    } else if (strcmp(ss, "FLASHDBSIM") == 0) {
        gp->storage_system->type = FLASHDBSIM;
    } else {
        gp->storage_system->type = SSD;
    }

    if (strcmp(io, "DIRECT ACCESS") == 0) {
        gp->io_access = DIRECT_ACCESS;
    } else if (strcmp(io, "NORMAL ACCESS") == 0) {
        gp->io_access = NORMAL_ACCESS;
    } else {
        gp->io_access = DIRECT_ACCESS;
    }

    if (strcmp(r, "ONLY GEOS") == 0) {
        gp->refinement_type = ONLY_GEOS;
    } else {
        gp->refinement_type = GEOS_AND_POINT_POLYGON;
    }

    /*we have to read the information for the flashdbsim simulator*/
    if (gp->storage_system->type == FLASHDBSIM) {
        MemoryContext old_context;
        FlashDBSim *flashdbsim;
        int nand_device_type, block_count, page_count_per_block,
                page_size1, page_size2, erase_limitation, read_random_time,
                read_serial_time, program_time, erase_time,
                ftl_type, map_list_size, wear_leveling_threshold;
        char optional_query[512];

        sprintf(optional_query, "SELECT nand_device_type, block_count, page_count_per_block, "
                "page_size1, page_size2, erase_limitation, read_random_time, "
                "read_serial_time, program_time, erase_time, "
                "ftl_type, map_list_size, wear_leveling_threshold "
                "FROM fds.virtualflashdevice as vfd, fds.flashtranslationlayer as ftl, fds.FlashDBSimConfiguration as f "
                "WHERE vfd.vfd_id = f.vfd_id AND ftl.ftl_id = f.ftl_id AND ss_id = %d;", gp->storage_system->ss_id);

        err = SPI_execute(optional_query, true, 1);

        if (err < 0) {
            SPI_finish();
            _DEBUG(ERROR, "read_generic_parameters_from_fds: could not execute the SELECT command");
            return NULL;
        }

        if (SPI_processed <= 0) {
            SPI_finish();
            _DEBUGF(ERROR, "the ss_id (%d) does not exist in the FlashDBSimConfiguration table", bc_id);
            return NULL;
        }

        nand_device_type = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));
        block_count = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2));
        page_count_per_block = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 3));
        page_size1 = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4));
        page_size2 = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 5));
        erase_limitation = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 6));
        read_random_time = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 7));
        read_serial_time = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 8));
        program_time = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 9));
        erase_time = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 10));
        ftl_type = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 11));
        map_list_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 12));
        wear_leveling_threshold = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 13));

        /*because of the allocations for the FLASHDBSim*/
        old_context = MemoryContextSwitchTo(TopMemoryContext);
        flashdbsim = (FlashDBSim*) lwalloc(sizeof (FlashDBSim));
        flashdbsim->nand_device_type = nand_device_type;
        flashdbsim->block_count = block_count;
        flashdbsim->erase_limitation = erase_limitation;
        flashdbsim->erase_time = erase_time;
        flashdbsim->ftl_type = ftl_type;
        flashdbsim->map_list_size = map_list_size;
        flashdbsim->page_count_per_block = page_count_per_block;
        flashdbsim->page_size1 = page_size1;
        flashdbsim->page_size2 = page_size2;
        flashdbsim->program_time = program_time;
        flashdbsim->read_random_time = read_random_time;
        flashdbsim->read_serial_time = read_serial_time;
        flashdbsim->wear_leveling_threshold = wear_leveling_threshold;

        gp->storage_system->info = (void*) flashdbsim;

        MemoryContextSwitchTo(old_context);
    } else {
        gp->storage_system->info = NULL;
    }

    //the result of the SPI_getvalue is returned in memory allocated using palloc
    //thus we have to free it
    pfree(ss);
    pfree(io);
    pfree(r);

    /* disconnect from SPI */
    SPI_finish();

    return gp;
}

Source *read_source_from_fds(int src_id) {
    char query[256];
    int err;
    MemoryContext old_context;

    char *p, *t, *c, *s;

    Source *src = (Source*) lwalloc(sizeof (Source));

    sprintf(query, "SELECT schema_name, table_name, column_name, pk_name "
            "FROM fds.source WHERE src_id = %d;", src_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "read_source_from_fds: could not connect to SPI manager");
        return NULL;
    }
    err = SPI_execute(query, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "read_source_from_fds: could not execute the SELECT command");
        return NULL;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        _DEBUGF(ERROR, "the src_id (%d) does not exist in the table", src_id);
        return NULL;
    }

    s = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
    t = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2);
    c = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 3);
    p = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4);

    /* get the variables in upper memory context (outside of SPI) */
    /* TODO: use a narrower context to switch to */
    old_context = MemoryContextSwitchTo(TopMemoryContext);
    src->schema = (char*) lwalloc(strlen(s) + 1);
    strcpy(src->schema, s);

    src->table = (char*) lwalloc(strlen(t) + 1);
    strcpy(src->table, t);

    src->column = (char*) lwalloc(strlen(c) + 1);
    strcpy(src->column, c);

    src->pk = (char*) lwalloc(strlen(p) + 1);
    strcpy(src->pk, p);

    src->src_id = src_id;

    MemoryContextSwitchTo(old_context);

    pfree(s);
    pfree(t);
    pfree(c);
    pfree(p);

    /* disconnect from SPI */
    SPI_finish();

    return src;
}

BufferSpecification *read_bufferconfiguration_from_fds(int buf_id, int page_size) {
    char query[256];
    int err;

    char *t;

    BufferSpecification *bs = (BufferSpecification*) lwalloc(sizeof (BufferSpecification));

    sprintf(query, "SELECT upper(buf_type), buf_size "
            "FROM fds.bufferconfiguration WHERE buf_id = %d;", buf_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "read_bufferconfiguration_from_fds: could not connect to SPI manager");
        return NULL;
    }
    err = SPI_execute(query, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "read_bufferconfiguration_from_fds: could not execute the SELECT command");
        return NULL;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        //then we return an "empty" buffer specification
        //this means that this index will not have a buffer
        bs->buffer_type = BUFFER_NONE;
        bs->max_capacity = 0;
        bs->min_capacity = 0;

        return bs;
    }

    t = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
    bs->min_capacity = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2));
    bs->max_capacity = bs->min_capacity;
    bs->buf_id = buf_id;

    if (strcmp(t, "NONE") == 0) {
        bs->buffer_type = BUFFER_NONE;
    } else if (strcmp(t, "LRU") == 0) {
        bs->buffer_type = BUFFER_LRU;
    } else if (strcmp(t, "HLRU") == 0) {
        bs->buffer_type = BUFFER_HLRU;
    } else if (strncmp(t, "S2Q", 3) == 0) {
        double a1_perc = -1.0;
        int nofpages;
        char *ptr;
        char *start;
        MemoryContext old_context;
        BufferS2QSpecification *spec_s2q;

        old_context = MemoryContextSwitchTo(TopMemoryContext);
        spec_s2q = (BufferS2QSpecification*) lwalloc(sizeof (BufferS2QSpecification));

        bs->buffer_type = BUFFER_S2Q;

        start = t;

        //now we read the parameter of the 2Q, which is provided in the following format
        //S2Q(A1_size_perc)
        t += 3;
        while (isspace(*t)) t++;
        if (*t == '(')
            t += 1; //parentheses
        else
            _DEBUGF(ERROR, "Invalid format (%s). "
                "Format to define the parameter of S2Q buffer is: S2Q(A1_size_perc)", start);
        //the number
        a1_perc = strtod(t, &ptr);
        t = ptr;

        while (isspace(*t)) t++;
        if (*t == ')')
            t += 1; //parentheses
        else
            _DEBUGF(ERROR, "Invalid format (%s). "
                "Format to define the parameter of S2Q buffer is: S2Q(A1_size_perc)", start);

        if (a1_perc < 0) {
            _DEBUGF(ERROR, "Value %f is not valid for the S2Q buffer", a1_perc);
        }

        //ok, we have valid parameters
        //here, the size of the s2q is fixed, we could change it in the future
        nofpages = (int) ceil(bs->max_capacity / (double) (page_size + sizeof (int)));
        spec_s2q->A1_size = (size_t) ((double) nofpages * (a1_perc / 100.0));
        spec_s2q->Am_size = bs->max_capacity;

        bs->buf_additional_param = (void*) spec_s2q;

        MemoryContextSwitchTo(old_context);
        t = start;
    } else if (strncmp(t, "2Q", 2) == 0) {
        double a1in_perc = -1.0, a1out_perc = -1.0;
        char *ptr;
        char *start;
        int nofpages;
        MemoryContext old_context;

        Buffer2QSpecification *spec_2q;
        old_context = MemoryContextSwitchTo(TopMemoryContext);


        spec_2q = (Buffer2QSpecification*) lwalloc(sizeof (Buffer2QSpecification));

        bs->buffer_type = BUFFER_2Q;

        start = t;

        //now we read the parameter of the 2Q, which is provided in the following format
        //2Q(A1in_size_perc, A1out_size_perc)
        t += 2;
        while (isspace(*t)) t++;
        if (*t == '(')
            t += 1; //parentheses
        else
            _DEBUGF(ERROR, "Invalid format (%s). "
                "Format to define the parameter of 2Q buffer is: 2Q(A1in_size_perc, A1out_size_perc)", start);
        //the number
        a1in_perc = strtod(t, &ptr);
        t = ptr;
        if (a1in_perc < 0) {
            _DEBUGF(ERROR, "Value %f is not valid for the 2Q buffer", a1in_perc);
        }
        while (isspace(*t)) t++;
        if (*t == ',')
            t += 1; //comma
        else
            _DEBUGF(ERROR, "Invalid format (%s). "
                "Format to define the parameter of 2Q buffer is: 2Q(A1in_size_perc, A1out_size_perc)", start);

        //the other number
        a1out_perc = strtod(t, &ptr);
        t = ptr;
        if (a1out_perc < 0) {
            _DEBUGF(ERROR, "Value %f is not valid for the 2Q buffer", a1out_perc);
        }

        while (isspace(*t)) t++;
        if (*t == ')')
            t += 1; //parentheses
        else
            _DEBUGF(ERROR, "Invalid format (%s). "
                "Format to define the parameter of 2Q buffer is: 2Q(A1in_size_perc, A1out_size_perc)", start);

        //ok, we have valid parameters
        //here, the size of the 2q is fixed, we could change it in the future
        spec_2q->A1in_size = (size_t) ((double) bs->max_capacity * (a1in_perc / 100.0));
        spec_2q->Am_size = bs->max_capacity - spec_2q->A1in_size;
        //A1out should hold identifiers for as many pages as would fit on a1out_perc% of the buffer. 
        //each identifier occupies sizeof(int), as always, we did not consider the size of underlying structures
        nofpages = (int) ceil((double) bs->max_capacity / (double) (page_size + sizeof (int)));
        spec_2q->A1out_size = (size_t) ((double) nofpages * (a1out_perc / 100.0));

        bs->buf_additional_param = (void*) spec_2q;

        MemoryContextSwitchTo(old_context);

        t = start;
    } else {
        SPI_finish();
        _DEBUGF(ERROR, "There is not this type of buffer: %s", t);
    }

    //the result of the SPI_getvalue is returned in memory allocated using palloc
    //thus we have to free it
    pfree(t);

    /* disconnect from SPI */
    SPI_finish();

    return bs;
}

void set_rtreespec_from_fds(RTreeSpecification *spec, int sc_id, int page_size) {
    int split;
    double max_fill_leaf_nodes;
    double max_fill_int_nodes;
    double min_fill_leaf_nodes;
    double min_fill_int_nodes;

    char query[256];
    int err;
    char *s;

    sprintf(query, "SELECT upper(split_type), min_fill_int_nodes, "
            "min_fill_leaf_nodes, max_fill_int_nodes, max_fill_leaf_nodes, o.or_id "
            "FROM fds.rtreeconfiguration as c, fds.occupancyrate as o "
            "WHERE c.or_id = o.or_id AND sc_id = %d;", sc_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "read_rtreespec_from_fds: could not connect to SPI manager");
        return;
    }
    err = SPI_execute(query, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "read_rtreespec_from_fds: could not execute the SELECT command");
        return;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        _DEBUGF(ERROR, "the sc_id (%d) does not exist in the table", sc_id);
        return;
    }

    s = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
    min_fill_int_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2));
    min_fill_leaf_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 3));
    max_fill_int_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4));
    max_fill_leaf_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 5));
    spec->or_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 6));

    if (strcmp(s, "EXPONENTIAL") == 0) {
        split = RTREE_EXPONENTIAL_SPLIT;
    } else if (strcmp(s, "LINEAR") == 0) {
        split = RTREE_LINEAR_SPLIT;
    } else if (strcmp(s, "QUADRATIC") == 0) {
        split = RTREE_QUADRATIC_SPLIT;
    } else if (strcmp(s, "RSTARTREE SPLIT") == 0) {
        split = RSTARTREE_SPLIT;
    } else if (strcmp(s, "GREENE SPLIT") == 0) {
        split = GREENE_SPLIT;
    } else if (strcmp(s, "ANGTAN SPLIT") == 0) {
        split = ANGTAN_SPLIT;
    } else {
        _DEBUGF(ERROR, "The split %s is not supported by FESTIval.", s);
    }

    /* disconnect from SPI */
    SPI_finish();

    spec->split_type = split;
    spec->max_entries_leaf_node = rtreesinfo_get_max_entries(CONVENTIONAL_RTREE,
            page_size, rentry_size(), max_fill_leaf_nodes / 100.0);
    spec->max_entries_int_node = rtreesinfo_get_max_entries(CONVENTIONAL_RTREE,
            page_size, rentry_size(), max_fill_int_nodes / 100.0);
    spec->min_entries_leaf_node = rtreesinfo_get_min_entries(CONVENTIONAL_RTREE,
            spec->max_entries_leaf_node, min_fill_leaf_nodes / 100.0);
    spec->min_entries_int_node = rtreesinfo_get_min_entries(CONVENTIONAL_RTREE,
            spec->max_entries_int_node, min_fill_int_nodes / 100.0);
}

void set_rstartreespec_from_fds(RStarTreeSpecification *rs, int sc_id, int page_size) {
    char query[512];
    int err;
    char *s;
    double max_fill_leaf_nodes;
    double max_fill_int_nodes;
    double min_fill_leaf_nodes;
    double min_fill_int_nodes;
    uint8_t rein_tp;

    sprintf(query, "SELECT reinsertion_perc_internal_node, reinsertion_perc_leaf_node, "
            "upper(reinsertion_type), max_neighbors_exam, min_fill_int_nodes, "
            "min_fill_leaf_nodes, max_fill_int_nodes, max_fill_leaf_nodes, o.or_id "
            "FROM fds.rstartreeconfiguration as c, fds.occupancyrate as o "
            "WHERE c.or_id = o.or_id AND sc_id = %d;", sc_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "read_rstartreespec_from_fds: could not connect to SPI manager");
        return;
    }
    err = SPI_execute(query, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "read_rstartreespec_from_fds: could not execute the SELECT command");
        return;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        _DEBUGF(ERROR, "the sc_id (%d) does not exist in the table", sc_id);
        return;
    }

    rs->reinsert_perc_internal_node = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));
    rs->reinsert_perc_leaf_node = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2));
    s = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 3);
    rs->max_neighbors_to_examine = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4));
    min_fill_int_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 5));
    min_fill_leaf_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 6));
    max_fill_int_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 7));
    max_fill_leaf_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 8));
    rs->or_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 9));

    if (strcmp(s, "FAR REINSERT") == 0) {
        rein_tp = FAR_REINSERT;
    } else {
        rein_tp = CLOSE_REINSERT;
    }

    pfree(s);

    /* disconnect from SPI */
    SPI_finish();

    rs->reinsert_type = rein_tp;
    rs->max_entries_leaf_node = rtreesinfo_get_max_entries(CONVENTIONAL_RSTARTREE,
            page_size, rentry_size(), max_fill_leaf_nodes / 100.0);
    rs->max_entries_int_node = rtreesinfo_get_max_entries(CONVENTIONAL_RSTARTREE,
            page_size, rentry_size(), max_fill_int_nodes / 100.0);
    rs->min_entries_leaf_node = rtreesinfo_get_min_entries(CONVENTIONAL_RSTARTREE,
            rs->max_entries_leaf_node, min_fill_leaf_nodes / 100.0);
    rs->min_entries_int_node = rtreesinfo_get_min_entries(CONVENTIONAL_RSTARTREE,
            rs->max_entries_int_node, min_fill_int_nodes / 100.0);
}

void set_hilbertrtreespec_from_fds(HilbertRTreeSpecification *spec, int sc_id, int page_size) {
    double max_fill_leaf_nodes;
    double max_fill_int_nodes;
    double min_fill_leaf_nodes;
    double min_fill_int_nodes;

    char query[256];
    int err;

    sprintf(query, "SELECT order_splitting_policy, min_fill_int_nodes, "
            "min_fill_leaf_nodes, max_fill_int_nodes, max_fill_leaf_nodes, o.or_id "
            "FROM fds.hilbertrtreeconfiguration as c, fds.occupancyrate as o "
            "WHERE c.or_id = o.or_id AND sc_id = %d;", sc_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "read_hilbertrtreespec_from_fds: could not connect to SPI manager");
        return;
    }
    err = SPI_execute(query, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "read_hilbertrtreespec_from_fds: could not execute the SELECT command");
        return;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        _DEBUGF(ERROR, "the sc_id (%d) does not exist in the table", sc_id);
        return;
    }

    spec->order_splitting_policy = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));
    min_fill_int_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2));
    min_fill_leaf_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 3));
    max_fill_int_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4));
    max_fill_leaf_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 5));
    spec->or_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 6));
    //we first set it to 0 first, after the first insertion, the srid will be set and will have the same value
    spec->srid = 0;

    /* disconnect from SPI */
    SPI_finish();

    spec->max_entries_leaf_node = rtreesinfo_get_max_entries(CONVENTIONAL_HILBERT_RTREE,
            page_size, rentry_size(), max_fill_leaf_nodes / 100.0);
    spec->max_entries_int_node = rtreesinfo_get_max_entries(CONVENTIONAL_HILBERT_RTREE,
            page_size, hilbertientry_size(), max_fill_int_nodes / 100.0);
    spec->min_entries_leaf_node = rtreesinfo_get_min_entries(CONVENTIONAL_HILBERT_RTREE,
            spec->max_entries_leaf_node, min_fill_leaf_nodes / 100.0);
    spec->min_entries_int_node = rtreesinfo_get_min_entries(CONVENTIONAL_HILBERT_RTREE,
            spec->max_entries_int_node, min_fill_int_nodes / 100.0);

    if (spec->order_splitting_policy > spec->min_entries_int_node || spec->order_splitting_policy > spec->min_entries_leaf_node) {
        _DEBUG(ERROR, "The order splitting policy cannot be greater than the minimum entries allowed in a node.");
    }
}

FASTSpecification *set_fastspec_from_fds(int sc_id, int *index_type) {
    char query[256];
    int err;
    char *s;
    char *i;

    FASTSpecification *ret = (FASTSpecification*) lwalloc(sizeof (FASTSpecification));

    sprintf(query, "SELECT upper(index_type), db_sc_id, "
            "buffer_size, flushing_unit_size, upper(flushing_policy), log_size "
            "FROM fds.fastconfiguration WHERE sc_id = %d;", sc_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "read_fastspec_from_fds: could not connect to SPI manager");
        return NULL;
    }
    err = SPI_execute(query, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "read_fastspec_from_fds: could not execute the SELECT command");
        return NULL;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        _DEBUGF(ERROR, "the sc_id (%d) does not exist in the table", sc_id);
        return NULL;
    }

    i = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
    ret->index_sc_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2));
    ret->buffer_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 3));
    ret->flushing_unit_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4));
    s = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 5);
    ret->log_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 6));

    if (strcmp(s, "FLUSH ALL") == 0) {
        ret->flushing_policy = FLUSH_ALL;
    } else if (strcmp(s, "RANDOM FLUSH") == 0) {
        ret->flushing_policy = RANDOM_FLUSH;
    } else if (strcmp(s, "FAST FLUSHING POLICY") == 0) {
        ret->flushing_policy = FAST_FLUSHING_POLICY;
    } else if (strcmp(s, "FAST STAR FLUSHING POLICY") == 0) {
        ret->flushing_policy = FAST_STAR_FLUSHING_POLICY;
    } else {
        ret->flushing_policy = FAST_STAR_FLUSHING_POLICY; //default value
    }

    if (strcmp(i, "RTREE") == 0) {
        *index_type = FAST_RTREE_TYPE;
    } else if (strcmp(i, "RSTARTREE") == 0) {
        *index_type = FAST_RSTARTREE_TYPE;
    } else if (strcmp(i, "HILBERT RTREE") == 0) {
        *index_type = FAST_HILBERT_RTREE_TYPE;
    } else {
        *index_type = 0;
    }

    pfree(s);
    pfree(i);

    /* disconnect from SPI */
    SPI_finish();

    ret->offset_last_elem_log = 0;
    ret->size_last_elem_log = 0;

    return ret;
}

FORTreeSpecification *set_fortreespec_from_fds(int sc_id, int page_size) {
    char query[512];
    int err;
    double max_fill_leaf_nodes;
    double max_fill_int_nodes;
    double min_fill_leaf_nodes;
    double min_fill_int_nodes;

    FORTreeSpecification *ret = (FORTreeSpecification*) lwalloc(sizeof (FORTreeSpecification));

    sprintf(query, "SELECT buffer_size, flushing_unit_size, ratio_flushing, x, y, "
            "min_fill_int_nodes, min_fill_leaf_nodes, "
            "max_fill_int_nodes, max_fill_leaf_nodes, o.or_id "
            "FROM fds.fortreeconfiguration as c, fds.occupancyrate as o "
            "WHERE c.or_id = o.or_id AND sc_id = %d;", sc_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "read_fortreespec_from_fds: could not connect to SPI manager");
        return NULL;
    }
    err = SPI_execute(query, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "read_fortreespec_from_fds: could not execute the SELECT command");
        return NULL;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        _DEBUGF(ERROR, "the sc_id (%d) does not exist in the table", sc_id);
        return NULL;
    }

    ret->buffer_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));
    ret->flushing_unit_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2));
    ret->ratio_flushing = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 3));
    ret->x = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4));
    ret->y = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 5));
    min_fill_int_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 6));
    min_fill_leaf_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 7));
    max_fill_int_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 8));
    max_fill_leaf_nodes = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 9));
    ret->or_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 10));

    /* disconnect from SPI */
    SPI_finish();

    ret->max_entries_leaf_node = rtreesinfo_get_max_entries(FORTREE_TYPE,
            page_size, rentry_size(), max_fill_leaf_nodes / 100.0);
    ret->max_entries_int_node = rtreesinfo_get_max_entries(FORTREE_TYPE,
            page_size, rentry_size(), max_fill_int_nodes / 100.0);
    ret->min_entries_leaf_node = rtreesinfo_get_min_entries(FORTREE_TYPE,
            ret->max_entries_leaf_node, min_fill_leaf_nodes / 100.0);
    ret->min_entries_int_node = rtreesinfo_get_min_entries(FORTREE_TYPE,
            ret->max_entries_int_node, min_fill_int_nodes / 100.0);

    return ret;
}

eFINDSpecification *set_efindspec_from_fds(int sc_id, int *index_type) {
    char query[512];
    int err;
    char *s;
    char *t;
    char *i;
    char *r;
    int buffer_size;

    eFINDSpecification *ret = (eFINDSpecification*) lwalloc(sizeof (eFINDSpecification));

    sprintf(query, "SELECT upper(index_type), db_sc_id, "
            "buffer_size, read_buffer_perc, "
            "upper(temporal_control_policy), "
            "read_temporal_control_perc, "
            "write_temporal_control_size, write_temporal_control_mindist, write_temporal_control_stride, "
            "timestamp_percentage, flushing_unit_size, upper(flushing_policy), log_size, upper(read_buffer_policy) "
            "FROM fds.efindconfiguration WHERE sc_id = %d;", sc_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "read_efindspec_from_fds: could not connect to SPI manager");
        return NULL;
    }
    err = SPI_execute(query, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "read_efindspec_from_fds: could not execute the SELECT command");
        return NULL;
    }

    if (SPI_processed <= 0) {
        SPI_finish();
        _DEBUGF(ERROR, "the sc_id (%d) does not exist in the table", sc_id);
        return NULL;
    }

    i = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1);
    ret->index_sc_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 2));
    buffer_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 3));
    ret->read_buffer_perc = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4));
    t = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 5);
    ret->read_temporal_control_perc = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 6));

    ret->write_temporal_control_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 7));
    ret->write_tc_minimum_distance = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 8));
    ret->write_tc_stride = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 9));

    ret->timestamp_perc = atof(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 10));
    ret->flushing_unit_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 11));
    s = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 12);
    ret->log_size = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 13));
    r = SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 14);

    if (strcmp(t, "TEMPORAL CONTROL NONE") == 0) {
        ret->temporal_control_policy = eFIND_NONE_TCP;
    } else if (strcmp(t, "TEMPORAL CONTROL FOR READS") == 0) {
        ret->temporal_control_policy = eFIND_READ_TCP;
    } else if (strcmp(t, "TEMPORAL CONTROL FOR WRITES") == 0) {
        ret->temporal_control_policy = eFIND_WRITE_TCP;
    } else if (strcmp(t, "TEMPORAL CONTROL FOR READS AND WRITES") == 0) {
        ret->temporal_control_policy = eFIND_READ_WRITE_TCP;
    } else {
        _DEBUGF(ERROR, "Unknown temporal control policy %s for the eFIND index", t);
    }

    if (strcmp(s, "EFIND FLUSH MOD") == 0) {
        ret->flushing_policy = eFIND_M_FP;
    } else if (strcmp(s, "EFIND FLUSH MOD TIME") == 0) {
        ret->flushing_policy = eFIND_MT_FP;
    } else if (strcmp(s, "EFIND FLUSH MOD TIME HEIGHT") == 0) {
        ret->flushing_policy = eFIND_MTH_FP;
    } else if (strcmp(s, "EFIND FLUSH MOD TIME HEIGHT AREA") == 0) {
        ret->flushing_policy = eFIND_MTHA_FP;
    } else if (strcmp(s, "EFIND FLUSH MOD TIME HEIGHT AREA OVERLAP") == 0) {
        ret->flushing_policy = eFIND_MTHAO_FP;
    } else {
        _DEBUGF(ERROR, "Unknown flushing policy %s for the eFIND index", s);
    }

    if (strcmp(i, "RTREE") == 0) {
        *index_type = eFIND_RTREE_TYPE;
    } else if (strcmp(i, "RSTARTREE") == 0) {
        *index_type = eFIND_RSTARTREE_TYPE;
    } else if (strcmp(i, "HILBERT RTREE") == 0) {
        *index_type = eFIND_HILBERT_RTREE_TYPE;
    } else {
        *index_type = 0;
    }

    if (strcmp(r, "NONE") == 0) {
        ret->read_buffer_policy = eFIND_NONE_RBP;
        if (ret->read_buffer_perc > 0) {
            _DEBUGF(ERROR, "You should choose a read buffer policy since "
                    "your read buffer percentage is equal to %f percent.", ret->read_buffer_perc);
        }
    } else if (strcmp(r, "EFIND LRU") == 0) {
        ret->read_buffer_policy = eFIND_LRU_RBP;
    } else if (strcmp(r, "EFIND HLRU") == 0) {
        ret->read_buffer_policy = eFIND_HLRU_RBP;
    } else if (strcmp(r, "EFIND S2Q") == 0) {
        ret->read_buffer_policy = eFIND_S2Q_RBP;
    } else if (strncmp(r, "EFIND 2Q", 8) == 0) {
        char *start;
        char *ptr;
        MemoryContext old_context;

        eFIND2QSpecification *spec_2q;
        old_context = MemoryContextSwitchTo(TopMemoryContext);

        spec_2q = (eFIND2QSpecification*) lwalloc(sizeof (eFIND2QSpecification));

        start = r;

        spec_2q->A1in_perc_size = -1;
        ret->read_buffer_policy = eFIND_2Q_RBP;

        //now we read the parameter of the 2Q, which is provided in the following format
        //the A1out parameter is out read_temporal_control_perc
        //eFIND 2Q(param)
        r += 8;
        while (isspace(*r)) r++;
        if (*r == '(')
            r += 1; //parentheses
        else
            _DEBUGF(ERROR, "Invalid format (%s). "
                "Format to define the parameter of 2Q buffer for eFIND is: eFIND 2Q(param)", start);

        //the number
        spec_2q->A1in_perc_size = strtod(r, &ptr);
        r = ptr;

        if (spec_2q->A1in_perc_size < 0) {
            _DEBUGF(ERROR, "Value %f is not valid for the 2Q buffer of eFIND", spec_2q->A1in_perc_size);
        }
        while (isspace(*r)) r++;
        if (*r == ')')
            r += 1; //parentheses
        else
            _DEBUGF(ERROR, "Invalid format (%s). "
                "Format to define the parameter of 2Q buffer for eFIND is: eFIND 2Q(param)", start);

        ret->rbp_additional_params = (void*) spec_2q;

        MemoryContextSwitchTo(old_context);

        r = start;
    } else {
        _DEBUGF(ERROR, "Unknown read buffer policy %s for the eFIND index", r);
    }

    pfree(s);
    pfree(i);
    pfree(t);
    pfree(r);

    /* disconnect from SPI */
    SPI_finish();

    //buffer size
    ret->read_buffer_size = (int) ((double) buffer_size * (ret->read_buffer_perc / 100.0));
    ret->write_buffer_size = buffer_size - ret->read_buffer_size;
    ret->offset_last_elem_log = 0;
    ret->size_last_elem_log = 0;

    //temporal control for writes size
    ret->write_temporal_control_size = ret->write_temporal_control_size * ret->flushing_unit_size;

    return ret;
}

/*FUNCTIONS TO MANAGE STATISTICAL DATA*/
/*we reset all statistical data in order to start a new workload*/
Datum STI_start_collect_statistical_data(PG_FUNCTION_ARGS);
/*we store all statistical data after a set of operations - it returns the execution id*/
Datum STI_store_collected_statistical_data(PG_FUNCTION_ARGS);
/*we collect and store only the index_snapshot - it receives and execution id as parameter*/
Datum STI_store_index_snapshot(PG_FUNCTION_ARGS);
/*a function that sets the execution name IN MEMORY -> this function has priority to define the execution_name*/
Datum STI_set_execution_name(PG_FUNCTION_ARGS);
/* a function to indicates that we want to collect the read and write order*/
Datum STI_collect_read_write_order(PG_FUNCTION_ARGS);

/* one function for each type of operation */

/*create an empty index according to a set of parameters from the statistical schema*/
Datum STI_create_empty_index(PG_FUNCTION_ARGS);
/* finish the creation of a FAST and FOR-tree indices, which are flash-aware indices (FAI)*/
Datum STI_finish_fai(PG_FUNCTION_ARGS);
/* apply all the modified nodes stored in the standard buffer (e.g., LRU) and clean it*/
Datum STI_finish_buffer(PG_FUNCTION_ARGS);
/*insert an entry in a created spatial index - an usual operation*/
Datum STI_insert_entry(PG_FUNCTION_ARGS);
/* remove an entry from a created spatial index */
Datum STI_remove_entry(PG_FUNCTION_ARGS);
/* update an entry (it is a sequence of remotion */
Datum STI_update_entry(PG_FUNCTION_ARGS);
/*execute a query on a constructed spatial index*/
Datum STI_query_spatial_index(PG_FUNCTION_ARGS);


/*variables to collect times in order to guarantee the total elapsed time*/
static struct timespec _cpustart;
static struct timespec _start;

PG_FUNCTION_INFO_V1(STI_start_collect_statistical_data);

Datum STI_start_collect_statistical_data(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA

    /*this statistic option refers to the storage or not of the order of performed reads/writes*/
    int statistic_options = PG_GETARG_INT32(0);
    MemoryContext oldcontext;
    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    if (statistic_options == 0)
        _COLLECT_READ_WRITE_ORDER = 0;
    else
        _COLLECT_READ_WRITE_ORDER = 1;

    //we also free any previous statistical data
    statistic_free_allocated_memory();
    //we need also to reset the corresponding variables
    statistic_reset_variables();
    //we need to initiate the statistic values
    initiate_statistic_values();

    _cpustart = get_CPU_time();
    _start = get_current_time();

    MemoryContextSwitchTo(oldcontext);
#endif   
    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(STI_collect_read_write_order);

Datum STI_collect_read_write_order(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA

    MemoryContext oldcontext;
    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    _COLLECT_READ_WRITE_ORDER = 1;

    MemoryContextSwitchTo(oldcontext);
#endif   
    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(STI_store_collected_statistical_data);

Datum STI_store_collected_statistical_data(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA
    SpatialIndex *si;
    MemoryContext oldcontext;
    struct timespec cpuend;
    struct timespec end;

    char *index_name;
    char *index_path;
    //this is a flag to check that data we will compute
    int statistic_options = PG_GETARG_INT32(2);
    //this is an optional flag to indicate where to store the statistical data
    int location_statistics = PG_GETARG_INT32(3);
    char *statistic_file = NULL; //if location_statistics is equal to 2, this value should be informed
    uint8_t variant = 0;

    char *spc_path;
    int execution_id;

    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    variant = variant | SO_EXECUTION; //default (collect statistical data related to the execution table)

    /*
     * 1 - indicates that we will store the statistical data directly in the FESTIval's data schema
     * 2 - indicates that we will store the statistical data in a file, which will be the parameter 4.
     */
    if (location_statistics != 1 && location_statistics != 2) {
        _DEBUG(ERROR, "Invalid location to store statistical data");
    }

    if (location_statistics == 2) {
        //todo improve this checking.
        if (PG_ARGISNULL(4))
            _DEBUG(ERROR, "You must inform the complete path of a file in order to store the statistical data!");
        variant |= SO_STORE_STATISTICAL_IN_FILE;

        statistic_file = text_to_cstring(PG_GETARG_TEXT_PP(4));        
    }

    /*(1 for collect and store only data for Execution table, 
    2 for collect and store data for Execution and IndexSnapshot, 
     * 3 for collect and store data for Execution and PrintIndex, 
     * and 4 for collect and store data for Execution, IndexSnapshot, and PrintIndex)*/
    //the following options store also the flash simulation results and...
    /*(5 for collect and store only data for Execution table, 
    6 for collect and store data for Execution and IndexSnapshot, 
     * 7 for collect and store data for Execution and PrintIndex, 
     * and 8 for collect and store data for Execution, IndexSnapshot, and PrintIndex)*/
    if (statistic_options == 2) {
        variant |= SO_INDEXSNAPSHOT;
    } else if (statistic_options == 3) {
        variant |= SO_PRINTINDEX;
    } else if (statistic_options == 4) {
        variant |= SO_INDEXSNAPSHOT;
        variant |= SO_PRINTINDEX;
    } else if (statistic_options == 5) {
        variant |= SO_FLASHSIMULATOR;
    } else if (statistic_options == 6) {
        variant |= SO_INDEXSNAPSHOT;
        variant |= SO_FLASHSIMULATOR;
    } else if (statistic_options == 7) {
        variant |= SO_PRINTINDEX;
        variant |= SO_FLASHSIMULATOR;
    } else if (statistic_options == 8) {
        variant |= SO_INDEXSNAPSHOT;
        variant |= SO_PRINTINDEX;
        variant |= SO_FLASHSIMULATOR;
    }

    index_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
    index_path = text_to_cstring(PG_GETARG_TEXT_PP(1)); 

    cpuend = get_CPU_time();
    end = get_current_time();

    _total_cpu_time = get_elapsed_time(_cpustart, cpuend);
    _total_time = get_elapsed_time(_start, end);

    spc_path = lwalloc(strlen(index_name) + strlen(index_path) + strlen(".header") + 1);
    strcpy(spc_path, index_path);
    strcat(spc_path, index_name);
    strcat(spc_path, ".header");
    /*we do not take into account the overhead to get statistical data!*/
    _STORING = 1;

    si = spatialindex_from_header(spc_path);

    //_DEBUGF(NOTICE, "collecting statistic info %s, %s", index_path, index_name);

    /*execution_id is ALWAYS equal to 0 if (variant & SO_STORE_STATISTICAL_IN_FILE) */
    execution_id = process_statistic_information(si, variant, statistic_file);

    //_DEBUG(NOTICE, "done");

    _STORING = 0;
    _COLLECT_READ_WRITE_ORDER = 0;

    /*clean up the used memory*/
    lwfree(spc_path);
    lwfree(index_path);
    lwfree(index_name);

    MemoryContextSwitchTo(oldcontext);

    PG_RETURN_INT32(execution_id);
#endif 
}

PG_FUNCTION_INFO_V1(STI_store_index_snapshot);

/*a restriction for calling this function is that the execution_id is necessary!
 this means that, the method of storing statistical data must be equal to 1 (i.e., to store directly in the FESTIval's data schema)*/
Datum STI_store_index_snapshot(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA
    SpatialIndex *si;
    MemoryContext oldcontext;

    char *index_name;
    char *index_path;
    //the execution_id in order to know which execution this snapshot refers to
    int execution_id = PG_GETARG_INT32(2);
    //will we also store the print index information?
    bool print_index = PG_GETARG_BOOL(3);
    //this is an optional flag to indicate where to store the statistical data
    int location_statistics = PG_GETARG_INT32(4);
    char *statistic_file = NULL; //if location_statistics is equal to 2, this value should be informed
    uint8_t variant = 0;

    char *spc_path;

    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    /*(1 for collect and store only data for Execution table, 
    2 for collect and store data for Execution and IndexSnapshot, 
     * 3 for collect and store data for Execution and PrintIndex, 
     * and 4 for collect and store data for Execution, IndexSnapshot, and PrintIndex)*/
    variant = variant | SO_INDEXSNAPSHOT;

    /*
     * 1 - indicates that we will store the statistical data directly in the FESTIval's data schema
     * 2 - indicates that we will store the statistical data in a file, which will be the parameter 4.
     */
    if (location_statistics != 1 && location_statistics != 2) {
        _DEBUG(ERROR, "Invalid location to store statistical data");
    }

    if (location_statistics == 2) {
        if (PG_ARGISNULL(4))
            _DEBUG(ERROR, "You must inform the complete path of a file in order to store the statistical data!");
        variant |= SO_STORE_STATISTICAL_IN_FILE;

    	statistic_file = text_to_cstring(PG_GETARG_TEXT_PP(4));
    }

    if (print_index) {
        variant |= SO_PRINTINDEX;
    }

    index_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
    index_path = text_to_cstring(PG_GETARG_TEXT_PP(1));

    spc_path = lwalloc(strlen(index_name) + strlen(index_path) + strlen(".header") + 1);
    strcpy(spc_path, index_path);
    strcat(spc_path, index_name);
    strcat(spc_path, ".header");
    /*we do not take into account the overhead to get statistical data!*/
    _STORING = 1;

    si = spatialindex_from_header(spc_path);

    process_index_snapshot(si, execution_id, variant, statistic_file);

    _STORING = 0;

    /*clean up the used memory*/
    lwfree(spc_path);
    lwfree(index_path);
    lwfree(index_name);

    MemoryContextSwitchTo(oldcontext);
#endif 
    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(STI_set_execution_name);

Datum STI_set_execution_name(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA
    MemoryContext oldcontext;

    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);
    if (PG_ARGISNULL(0)) {
        _DEBUG(ERROR, "You must inform a valid execution name!");
    }

    if (_execution_name != NULL) {
        //we free it and then get the execution name
        lwfree(_execution_name);
        _execution_name = NULL;
    }

    _execution_name = text_to_cstring(PG_GETARG_TEXT_PP(0));

    MemoryContextSwitchTo(oldcontext);
#endif 
    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(STI_create_empty_index);

Datum STI_create_empty_index(PG_FUNCTION_ARGS) {
    int type = PG_GETARG_INT32(0);
    text *index_name_t = PG_GETARG_TEXT_P(1);
    text *index_path_t = PG_GETARG_TEXT_P(2);
    int src_id = PG_GETARG_INT32(3);
    int bc_id = PG_GETARG_INT32(4);
    int sc_id = PG_GETARG_INT32(5);
    int buf_id;

    char *index_name, *index_path;
    char *spc_path, *index_file;

    SpatialIndex *si = NULL;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;

    MemoryContext oldcontext;
    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    //validation - improve it later
    if (type != CONVENTIONAL_RTREE && type != CONVENTIONAL_RSTARTREE &&
            type != CONVENTIONAL_HILBERT_RTREE &&
            type != FAST_RTREE_TYPE && type != FAST_RSTARTREE_TYPE &&
            type != FAST_HILBERT_RTREE_TYPE &&
            type != FORTREE_TYPE && type != eFIND_RTREE_TYPE
            && type != eFIND_RSTARTREE_TYPE &&
            type != eFIND_HILBERT_RTREE_TYPE) {
        ereport(ERROR,
                (errcode(ERRCODE_CASE_NOT_FOUND),
                errmsg("There is no this index type - %d", type)));
    }

    if (PG_ARGISNULL(6)) {
        buf_id = 1;
    } else {
        buf_id = PG_GETARG_INT32(6);
    }

    index_name = text_to_cstring(index_name_t);
    index_path = text_to_cstring(index_path_t);

    /* we get the generic parameters */
    gp = read_basicconfiguration_from_fds(bc_id);
    /* we retrieve the information about the source data */
    src = read_source_from_fds(src_id);
    /* we get the buffer specification */
    bs = read_bufferconfiguration_from_fds(buf_id, gp->page_size);

    //we create the index_file
    index_file = lwalloc(strlen(index_name) + strlen(index_path) + 1);
    strcpy(index_file, index_path);
    strcat(index_file, index_name);
    //we determine the name of the header
    spc_path = lwalloc(strlen(index_file) + strlen(".header") + 1);
    strcpy(spc_path, index_file);
    strcat(spc_path, ".header");

    /*if required, we initialize some flash simulator*/
    check_flashsimulator_initialization(gp->storage_system);

    /*****************
     * WE NOW CREATE THE REQUIRED SPATIAL INDEX
     *****************
     */
    if (type == CONVENTIONAL_RTREE) {
        RTree *rtree;

        //we persist an empty root node since we are creating an empty index
        si = rtree_empty_create(index_file, src, gp, bs, true);
        rtree = (void *) si;
        set_rtreespec_from_fds(rtree->spec, sc_id, gp->page_size);
    } else if (type == CONVENTIONAL_RSTARTREE) {
        RStarTree *r;

        //we persist an empty root node since we are creating an empty index
        si = rstartree_empty_create(index_file, src, gp, bs, true);
        r = (void *) si;
        set_rstartreespec_from_fds(r->spec, sc_id, gp->page_size);
    } else if (type == CONVENTIONAL_HILBERT_RTREE) {
        HilbertRTree *r;

        //we persist an empty root node since we are creating an empty index
        si = hilbertrtree_empty_create(index_file, src, gp, bs, true);
        r = (void *) si;
        set_hilbertrtreespec_from_fds(r->spec, sc_id, gp->page_size);
    } else if (type == FAST_RSTARTREE_TYPE ||
            type == FAST_RTREE_TYPE ||
            type == FAST_HILBERT_RTREE_TYPE) {
        FASTIndex *fi = NULL;
        FASTSpecification *fs;
        int index_type_ck;

        fs = set_fastspec_from_fds(sc_id, &index_type_ck);

        if (index_type_ck != type) {
            //todo free used memory
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                    errmsg("The index type of the first argument (%d) "
                    "is not compatible with the specific configuration (%d)", index_type_ck, type)));
        }

        fs->log_file = lwalloc(strlen(index_file) + strlen(".log") + 1);
        sprintf(fs->log_file, "%s.log", index_file);

        if (type == FAST_RSTARTREE_TYPE) {
            FASTRStarTree *rstar;
            si = fastrstartree_empty_create(index_file, src, gp, bs, fs, true);
            fi = (void *) si;
            rstar = fi->fast_index.fast_rstartree;
            rstar->rstartree->base.sc_id = sc_id;
            set_rstartreespec_from_fds(rstar->rstartree->spec, fs->index_sc_id, gp->page_size);
        } else if (type == FAST_RTREE_TYPE) {
            FASTRTree *r;
            si = fastrtree_empty_create(index_file, src, gp, bs, fs, true);
            fi = (void *) si;
            r = fi->fast_index.fast_rtree;
            r->rtree->base.sc_id = sc_id;
            set_rtreespec_from_fds(r->rtree->spec, fs->index_sc_id, gp->page_size);
        } else if (type == FAST_HILBERT_RTREE_TYPE) {
            FASTHilbertRTree *r;
            si = fasthilbertrtree_empty_create(index_file, src, gp, bs, fs, true);
            fi = (void *) si;
            r = fi->fast_index.fast_hilbertrtree;
            r->hilbertrtree->base.sc_id = sc_id;
            set_hilbertrtreespec_from_fds(r->hilbertrtree->spec, fs->index_sc_id, gp->page_size);
        }
    } else if (type == FORTREE_TYPE) {
        FORTreeSpecification *spec;
        spec = set_fortreespec_from_fds(sc_id, gp->page_size);
        si = fortree_empty_create(index_file, src, gp, bs, spec, true);
    } else if (type == eFIND_RSTARTREE_TYPE ||
            type == eFIND_RTREE_TYPE ||
            type == eFIND_HILBERT_RTREE_TYPE) {
        eFINDIndex *fi = NULL;
        eFINDSpecification *fs;
        int index_type_ck;

        fs = set_efindspec_from_fds(sc_id, &index_type_ck);

        if (index_type_ck != type) {
            //todo free used memory
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                    errmsg("The index type of the first argument (%d) "
                    "is not compatible with the specific configuration (%d)", index_type_ck, type)));
        }

        fs->log_file = lwalloc(strlen(index_file) + strlen(".log") + 1);
        sprintf(fs->log_file, "%s.log", index_file);

        if (type == eFIND_RSTARTREE_TYPE) {
            eFINDRStarTree *rstar;
            si = efindrstartree_empty_create(index_file, src, gp, bs, fs, true);
            fi = (void *) si;
            rstar = fi->efind_index.efind_rstartree;
            rstar->rstartree->base.sc_id = sc_id;
            set_rstartreespec_from_fds(rstar->rstartree->spec, fs->index_sc_id, gp->page_size);

            //we should set the sizes of the 2Q properly
            if (rstar->spec->read_buffer_policy == eFIND_2Q_RBP) {
                efind_readbuffer_2q_setsizes(rstar->spec, gp->page_size);
            }
        } else if (type == eFIND_RTREE_TYPE) {
            eFINDRTree *r;
            si = efindrtree_empty_create(index_file, src, gp, bs, fs, true);
            fi = (void *) si;
            r = fi->efind_index.efind_rtree;
            r->rtree->base.sc_id = sc_id;
            set_rtreespec_from_fds(r->rtree->spec, fs->index_sc_id, gp->page_size);

            //we should set the sizes of the 2Q properly
            if (r->spec->read_buffer_policy == eFIND_2Q_RBP) {
                efind_readbuffer_2q_setsizes(r->spec, gp->page_size);
            }
        } else if (type == eFIND_HILBERT_RTREE_TYPE) {
            eFINDHilbertRTree *r;
            si = efindhilbertrtree_empty_create(index_file, src, gp, bs, fs, true);
            fi = (void *) si;
            r = fi->efind_index.efind_hilbertrtree;
            r->hilbertrtree->base.sc_id = sc_id;
            set_hilbertrtreespec_from_fds(r->hilbertrtree->spec, fs->index_sc_id, gp->page_size);
        }
    } else {
        _DEBUG(ERROR, "Unknown index type at creation of the index");
    }
    //set the specialized configuration in our spatial index (for statistical purposes)
    si->sc_id = sc_id;

    /*we keep this info in the buffer of headers*/
    if (!spatialindex_keep_header(spc_path, si)) {
        //in this case, the header was not stored in the header buffer because it was already there
        //memory cleaning
        //check if it should be allowed or not...
        spatialindex_destroy(si);
    }

    lwfree(spc_path);
    lwfree(index_name);
    lwfree(index_path);

    MemoryContextSwitchTo(oldcontext);

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(STI_finish_fai);

Datum STI_finish_fai(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
#endif

    SpatialIndex *si;
    char *spc_path;

    char *index_name;
    char *index_path;

    uint8_t type;

    MemoryContext oldcontext;
    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    index_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
    index_path = text_to_cstring(PG_GETARG_TEXT_PP(1));

    spc_path = lwalloc(strlen(index_name) + strlen(index_path) + strlen(".header") + 1);
    strcpy(spc_path, index_path);
    strcat(spc_path, index_name);
    strcat(spc_path, ".header");

    si = spatialindex_from_header(spc_path);
    type = spatialindex_get_type(si);

#ifdef COLLECT_STATISTICAL_DATA
    /* we collect the index time by avoiding the overhead of the header!
     the header time is included in the total_time */
    cpustart = get_CPU_time();
    start = get_current_time();
#endif


    /*we need to apply all the buffer stored operations
    this processing is also considered in the statistical data since:
    we must construct a complete index here*/
    if (type == FAST_RSTARTREE_TYPE || type == FAST_RTREE_TYPE || type == FAST_HILBERT_RTREE_TYPE) {
        FASTIndex *fi;
        fi = (void *) si;
        if (type == FAST_RTREE_TYPE) {
            fast_flush_all(si, fi->fast_index.fast_rtree->spec);
        } else if (type == FAST_RSTARTREE_TYPE) {
            fast_flush_all(si, fi->fast_index.fast_rstartree->spec);
        } else {
            fast_flush_all(si, fi->fast_index.fast_hilbertrtree->spec);
        }
        //we destroy the buffers too (it is not really needed since we remove the nodes in the buffer after the flushing)
        //    fast_destroy_flushing();
        //    fb_destroy_buffer(type);
    } else if (type == FORTREE_TYPE) {
        FORTree *f;
        f = (void *) si;

        //_DEBUG(NOTICE, "Flushing all FORTREE");

        forb_flushing_all(&f->base, f->spec);

        //_DEBUG(NOTICE, "Cleaning the buffer"); (it is not really needed since we remove the nodes in the buffer after the flushing)
        //we destroy the buffers too
        //forb_destroy_buffer();
    }
    if (type == eFIND_RSTARTREE_TYPE || type == eFIND_RTREE_TYPE || type == eFIND_HILBERT_RTREE_TYPE) {
        eFINDIndex *fi;
        fi = (void *) si;

        //_DEBUG(NOTICE, "flushing all the efind");
        if (type == eFIND_RTREE_TYPE) {
            efind_flushing_all(si, fi->efind_index.efind_rtree->spec);
        } else if (type == eFIND_RSTARTREE_TYPE) {
            efind_flushing_all(si, fi->efind_index.efind_rstartree->spec);
        } else {
            efind_flushing_all(si, fi->efind_index.efind_hilbertrtree->spec);
        }
        //_DEBUG(NOTICE, "done");

        //we destroy the write buffer too (it is not really needed since we remove the nodes in the buffer after the flushing)
        // efind_write_buf_destroy(type);

        //_DEBUG(NOTICE, "cleaned memory");
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _index_cpu_time += get_elapsed_time(cpustart, cpuend);
    _index_time += get_elapsed_time(start, end);
#endif

    /*we write the information of this index in a file
     we write here because of info of log files
     this is done here, because it is a flush all operation!*/
    spatialindex_header_writer(si, spc_path);

    //cleaning the used memory

    spatialindex_destroy(si); //in this case, SI should be freed

    lwfree(index_path);
    lwfree(index_name);
    lwfree(spc_path);

    MemoryContextSwitchTo(oldcontext);

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(STI_finish_buffer);

Datum STI_finish_buffer(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
#endif

    SpatialIndex *si;
    char *spc_path;

    char *index_name;
    char *index_path;

    MemoryContext oldcontext;
    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    index_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
    index_path = text_to_cstring(PG_GETARG_TEXT_PP(1));

    spc_path = lwalloc(strlen(index_name) + strlen(index_path) + strlen(".header") + 1);
    strcpy(spc_path, index_path);
    strcat(spc_path, index_name);
    strcat(spc_path, ".header");

    si = spatialindex_from_header(spc_path);

#ifdef COLLECT_STATISTICAL_DATA
    /* we collect the index time avoiding the overhead of the header!
     the header time is included in the total_time */
    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    /*we need to apply all the standard buffer stored operations
    this processing is also considered in the statistical data since:
    we must construct a complete index here*/
    storage_flush_all(si); //this operation also clean the used buffer

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _index_cpu_time += get_elapsed_time(cpustart, cpuend);
    _index_time += get_elapsed_time(start, end);
#endif

    //we write the header here because it is cached in the buffer
    //TODO put a flag in the in-memory buffer to decide whether the header should be written or not
    //in general we can call it here, because this SQL function will be called after other SQL functions
    spatialindex_header_writer(si, spc_path);

    //cleaning the used memory
    spatialindex_destroy(si); //in this case, we should free si
    lwfree(index_path);
    lwfree(index_name);
    lwfree(spc_path);

    MemoryContextSwitchTo(oldcontext);

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(STI_insert_entry);

Datum STI_insert_entry(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
#endif
    SpatialIndex *si;
    char *spc_path;

    char *index_name;
    char *index_path;

    int pointer = PG_GETARG_INT32(2);
    LWGEOM *lwgeom;
    GSERIALIZED *geom = PG_GETARG_GSERIALIZED_P(3);

    MemoryContext oldcontext;
    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    index_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
    index_path = text_to_cstring(PG_GETARG_TEXT_PP(1));

    lwgeom = lwgeom_from_gserialized(geom);

    spc_path = lwalloc(strlen(index_name) + strlen(index_path) + strlen(".header") + 1);
    strcpy(spc_path, index_path);
    strcat(spc_path, index_name);
    strcat(spc_path, ".header");

    /*
     ** See if we have a bounding box, add one if we don't have one.
     */
    if ((!lwgeom->bbox) && (!lwgeom_is_empty(lwgeom))) {
        lwgeom_add_bbox(lwgeom);
    }


    //_DEBUG(NOTICE, "getting the index");

    si = spatialindex_from_header(spc_path);

#ifdef COLLECT_STATISTICAL_DATA
    /* we collect the index time by avoiding the overhead of the header!
     the header time is included in the total_time */
    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    //_DEBUGF(NOTICE, "table %s, column %s, pk %s, schema %s", 
    //        si->src->table, si->src->column, si->src->pk, si->src->schema);
    //_DEBUGF(NOTICE, "page size %d, storage system %d, io access %d, refin type %d",
    //        si->gp->page_size, si->gp->storage_system, si->gp->io_access,
    //        si->gp->refinement_type);

    //_DEBUG(NOTICE, "inserting the entry");

    spatialindex_insert(si, pointer, lwgeom);

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _index_cpu_time += get_elapsed_time(cpustart, cpuend);
    _index_time += get_elapsed_time(start, end);
#endif

    //_DEBUG(NOTICE, "inserted the entry");

    //_DEBUG(NOTICE, "cleaning the memory");

    //cleaning the used memory 
    //we do not free si here because it is stored in the header buffer
    lwgeom_free(lwgeom);
    lwfree(index_name);
    lwfree(index_path);
    lwfree(spc_path);

    //_DEBUG(NOTICE, "memory cleaned");

    MemoryContextSwitchTo(oldcontext);

    PG_FREE_IF_COPY(geom, 3);
    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(STI_remove_entry);

Datum STI_remove_entry(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
#endif
    SpatialIndex *si;
    char *spc_path;

    char *index_name;
    char *index_path;

    int pointer = PG_GETARG_INT32(2);
    LWGEOM *lwgeom;
    GSERIALIZED *geom = PG_GETARG_GSERIALIZED_P(3);

    MemoryContext oldcontext;
    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    index_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
    index_path = text_to_cstring(PG_GETARG_TEXT_PP(1));

    lwgeom = lwgeom_from_gserialized(geom);

    spc_path = lwalloc(strlen(index_name) + strlen(index_path) + strlen(".header") + 1);
    strcpy(spc_path, index_path);
    strcat(spc_path, index_name);
    strcat(spc_path, ".header");

    /*
     ** See if we have a bounding box, add one if we don't have one.
     */
    if ((!lwgeom->bbox) && (!lwgeom_is_empty(lwgeom))) {
        lwgeom_add_bbox(lwgeom);
    }

    si = spatialindex_from_header(spc_path);

#ifdef COLLECT_STATISTICAL_DATA
    /* we collect the index time by avoiding the overhead of the header!
     the header time is included in the total_time */
    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    spatialindex_remove(si, pointer, lwgeom);

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _index_cpu_time += get_elapsed_time(cpustart, cpuend);
    _index_time += get_elapsed_time(start, end);
#endif
    
    //cleaning the used memory  
    lwgeom_free(lwgeom);
    lwfree(index_path);
    lwfree(index_name);
    lwfree(spc_path);

    MemoryContextSwitchTo(oldcontext);
    PG_FREE_IF_COPY(geom, 3);

    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(STI_update_entry);

Datum STI_update_entry(PG_FUNCTION_ARGS) {
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
#endif
    SpatialIndex *si;
    char *spc_path;

    char *index_name;
    char *index_path;

    int old_pointer = PG_GETARG_INT32(2);
    LWGEOM *old_lwgeom;

    int new_pointer = PG_GETARG_INT32(4);
    LWGEOM *new_lwgeom;

    GSERIALIZED *geom = PG_GETARG_GSERIALIZED_P(3);
    GSERIALIZED *geom2 = PG_GETARG_GSERIALIZED_P(5);

    MemoryContext oldcontext;
    // this will survives until a restart of the server
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);

    index_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
    index_path = text_to_cstring(PG_GETARG_TEXT_PP(1));

    old_lwgeom = lwgeom_from_gserialized(geom);
    new_lwgeom = lwgeom_from_gserialized(geom2);

    spc_path = lwalloc(strlen(index_name) + strlen(index_path) + strlen(".header") + 1);
    strcpy(spc_path, index_path);
    strcat(spc_path, index_name);
    strcat(spc_path, ".header");

    /*  
     ** See if we have a bounding box, add one if we don't have one.
     */
    if ((!old_lwgeom->bbox) && (!lwgeom_is_empty(old_lwgeom))) {
        lwgeom_add_bbox(old_lwgeom);
    }

    /*  
     ** See if we have a bounding box, add one if we don't have one.
     */
    if ((!new_lwgeom->bbox) && (!lwgeom_is_empty(new_lwgeom))) {
        lwgeom_add_bbox(new_lwgeom);
    }

    si = spatialindex_from_header(spc_path);

#ifdef COLLECT_STATISTICAL_DATA
    /* we collect the index time by avoiding the overhead of the header!
     the header time is included in the total_time */
    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    spatialindex_update(si, old_pointer, old_lwgeom, new_pointer, new_lwgeom);

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _index_cpu_time += get_elapsed_time(cpustart, cpuend);
    _index_time += get_elapsed_time(start, end);
#endif

    //cleaning the used memory    
    lwgeom_free(old_lwgeom);
    lwgeom_free(new_lwgeom);
    lwfree(index_path);
    lwfree(index_name);
    lwfree(spc_path);

    MemoryContextSwitchTo(oldcontext);

    PG_FREE_IF_COPY(geom, 3);
    PG_FREE_IF_COPY(geom2, 5);
    PG_RETURN_BOOL(true);
}

#define xpstrdup(tgtvar_, srcvar_) \
       do { \
           if (srcvar_) \
               tgtvar_ = pstrdup(srcvar_); \
           else \
               tgtvar_ = NULL; \
       } while (0)

/*index_name, index_path, type_query, query_bbox, predicate*/
PG_FUNCTION_INFO_V1(STI_query_spatial_index);

Datum STI_query_spatial_index(PG_FUNCTION_ARGS) {
    char *index_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
    char *index_path = text_to_cstring(PG_GETARG_TEXT_PP(1));

    int type_query = PG_GETARG_INT32(2);
    LWGEOM *lwgeom;
    GSERIALIZED *geom = PG_GETARG_GSERIALIZED_P(3);
    int predicate = PG_GETARG_INT32(4);
    int type_of_processing = PG_GETARG_INT32(5);
    char *spc_path;

    SpatialIndex *si;
    QueryResult *result;

    ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
    Tuplestorestate *tupstore;
    TupleDesc tupdesc;
    uint64 call_cntr;
    uint64 max_calls;
    MemoryContext per_query_ctx;
    MemoryContext oldcontext;

    /* check to see if caller supports us returning a tuplestore */
    if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
        ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
            errmsg("set-valued function called in context that cannot accept a set")));
    if (!(rsinfo->allowedModes & SFRM_Materialize))
        ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
            errmsg("materialize mode required, but it is not " \
   "allowed in this context")));

    per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;

    // this will survives until a restart of the server in order to collect statistical data
    oldcontext = MemoryContextSwitchTo(TopMemoryContext);
    lwgeom = lwgeom_from_gserialized(geom);

    /*checking if the input geometry is valid*/
    if (lwgeom_is_empty(lwgeom)) {
        _DEBUG(ERROR, "This is an empty geometry");
    }
    if (type_query == POINT_QUERY_TYPE) {
        if (lwgeom->type != POINTTYPE) {
            _DEBUGF(ERROR, "Invalid geometry type (%d) for the POINT_QUERY_TYPE", lwgeom->type);
        }
    } else if (type_query == RANGE_QUERY_TYPE) {
        //we have to check if it is a rectangle, otherwise we have to convert it to its BBOX
        if (lwgeom->type == POLYGONTYPE) {
            //we check if it is rectangular-shaped
            LWPOLY *poly = lwgeom_as_lwpoly(lwgeom);
            //here, we only check the number of rings (which must be equal to 1) and its number of points (=5)
            if (poly->nrings != 1 && poly->rings[0]->npoints != 5) {
                _DEBUG(ERROR, "Invalid geometry format for RANGE_QUERY_TYPE");
                //TO-DO perhaps we can have a better checker
            }
        } else {
            //we consider its bbox 
            LWGEOM *input;
            BBox *bbox = bbox_create();
            if ((!lwgeom->bbox)) {
                lwgeom_add_bbox(lwgeom);
            }
            gbox_to_bbox(lwgeom->bbox, bbox);
            input = bbox_to_geom(bbox);
            lwgeom_free(lwgeom);
            lwgeom = input;
            lwfree(bbox);
        }
    }

    spc_path = lwalloc(strlen(index_name) + strlen(index_path) + strlen(".header") + 1);
    strcpy(spc_path, index_path);
    strcat(spc_path, index_name);
    strcat(spc_path, ".header");

    si = spatialindex_from_header(spc_path);

    /*the index_time is collected inside this function
     the reason is that the query is processed in two steps: filtering and refinement*/
    result = process_spatial_selection(si, lwgeom, predicate, type_query, type_of_processing);

    lwgeom_free(lwgeom);
    lwfree(spc_path);
    lwfree(index_name);
    lwfree(index_path);

    //back to the current context
    MemoryContextSwitchTo(oldcontext);

    /*connect to SPI manager*/
    if (SPI_OK_CONNECT != SPI_connect()) {
        _DEBUG(ERROR, "could not connect to SPI manager");
    }

    /* get a tuple descriptor for our result type */
    switch (get_call_result_type(fcinfo, NULL, &tupdesc)) {
        case TYPEFUNC_COMPOSITE:
            /* success */
            break;
        case TYPEFUNC_RECORD:
            /* failed to determine actual type of RECORD */
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                    errmsg("function returning record called in context "
                    "that cannot accept type record")));
            break;
        default:
            /* result type isn't composite */
            ereport(ERROR,
                    (errcode(ERRCODE_DATATYPE_MISMATCH),
                    errmsg("return type must be a row type")));
            break;
    }

    /* switch to long-lived memory context
     */
    oldcontext = MemoryContextSwitchTo(per_query_ctx);

    /* make sure we have a persistent copy of the result tupdesc */
    tupdesc = CreateTupleDescCopy(tupdesc);

    /* initialize our tuplestore in long-lived context */
    tupstore = tuplestore_begin_heap(rsinfo->allowedModes & SFRM_Materialize_Random,
            false, 1024);

    MemoryContextSwitchTo(oldcontext);

    /* total number of tuples to be returned */
    max_calls = result->nofentries;

    for (call_cntr = 0; call_cntr < max_calls; call_cntr++) {
        HeapTuple tuple;

        Datum values[2];
        bool *nulls;

        /* build the tuple and store it */
        nulls = palloc(tupdesc->natts * sizeof ( bool));
        nulls[0] = false;

        values[0] = Int32GetDatum(result->row_id[call_cntr]);
        if (type_of_processing == FILTER_AND_REFINEMENT_STEPS && result->geoms[call_cntr]) {
            /*setting this column to be NOT NULL(It is very important)*/
            values[1] = PointerGetDatum(geometry_serialize(result->geoms[call_cntr]));
            nulls[1] = false;
        } else {
            values[1] = (Datum) 0;
            nulls[1] = true;
        }

        tuple = heap_form_tuple(tupdesc, values, nulls);
        pfree(nulls);
        tuplestore_puttuple(tupstore, tuple);

        heap_freetuple(tuple);
    }

    /* let the caller know we're sending back a tuplestore */
    rsinfo->returnMode = SFRM_Materialize;
    rsinfo->setResult = tupstore;
    rsinfo->setDesc = tupdesc;

    query_result_free(result, type_of_processing);

    PG_FREE_IF_COPY(geom, 3);
    /* release SPI related resources (and return to caller's context) */
    SPI_finish();

    return (Datum) 0;
}
