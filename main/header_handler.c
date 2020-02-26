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

#include "header_handler.h"

#include <sys/types.h>  /* required by open() */
#include <unistd.h>     /* open(), write() */
#include <fcntl.h>      /* open() and fcntl() */

#include "../libraries/uthash/uthash.h" //for hashing structures

#include "statistical_processing.h"
#include "storage_handler.h"
#include "log_messages.h" /* for log messages */

#include "../rtree/rtree.h" /* to create the RTREE for the spatial index */
#include "../rstartree/rstartree.h" /* to create the RSTARTREE for the spatial index */
#include "../hilbertrtree/hilbertrtree.h" /* to create the HILBERTRTREE for the spatial index */
#include "../fast/fast_buffer.h" /* to send request for buffer */
#include "../fast/fast_index.h" /* to create FAST-based indices */
#include "../fortree/fortree_buffer.h" /* to send requests for buffer */
#include "../fortree/fortree.h" /* to create FOR-Tree indices */
#include "../efind/efind_buffer_manager.h" /* to create eFIND-based indices */
#include "../efind/efind_read_buffer_policies.h" /* to initiate the  2Q read buffer */

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)

/*our buffer of SpatialIndex (the header) -- 
 * it does not store the root node, which should be retrieved*/
typedef struct HeaderBuffer {
    UT_hash_handle hh;

    char *path; //path of the header --> this is the key
    SpatialIndex *si;
} HeaderBuffer;

//this is our buffer
static HeaderBuffer *headers = NULL;

/*this function opens the specification file (.header)*/
static int hh_spc_open(const char *path);

static size_t hh_get_total_size(const char *path);

/*auxiliary functions to serialize, deserialize, and so on*/
static size_t hh_get_size_source(const Source *src);
static size_t hh_serialize_source(const Source *src, uint8_t *buf);
static Source *hh_get_source_from_serialization(uint8_t *buf, size_t *size);

static size_t hh_get_size_generic_spec(const GenericParameters* gp);
static size_t hh_serialize_generic_spec(const GenericParameters *gp, uint8_t *buf);
static GenericParameters *hh_get_generic_spec_from_serialization(uint8_t *buf, size_t *size);

static size_t hh_get_size_storage_system(const StorageSystem *ss);
static size_t hh_serialize_storage_system(const StorageSystem *ss, uint8_t *buf);
static StorageSystem *hh_get_storage_system_from_serialization(uint8_t *buf, size_t *size);

static size_t hh_get_size_buffer_spec(const BufferSpecification *bs);
static size_t hh_serialize_buffer_spec(const BufferSpecification *bs, uint8_t *buf);
static BufferSpecification *hh_get_buffer_spec_from_serialization(uint8_t *buf, size_t *size);

/*the next three functions are dedicated only for additional info, like the sc_id */
static size_t hh_get_size_other_param(void);
static size_t hh_serialize_other_param(const SpatialIndex *si, uint8_t *buf);
static int hh_get_sc_id(uint8_t *buf, size_t *size);

static size_t hh_get_size_rtrees_info(const RTreesInfo *info);
static size_t hh_serialize_rtrees_info(const RTreesInfo *info, uint8_t *buf);
static void hh_set_rtrees_info_from_serialization(RTreesInfo *info, uint8_t *buf, size_t *size);

/* these function serialize and read from serialization the RTREE Specification from the header file
 but before, we get the size of the specification*/
static size_t hh_get_size_rtreespec(void);
static size_t hh_serialize_rtreespec(const RTreeSpecification *spec, uint8_t *buf);
static void hh_set_rtreespec_from_serialization(RTreeSpecification *spec, uint8_t *buf, size_t *size);
/* these function serialize and read from serialization the RSTARTREE Specification from the header file
 but before, we get the size of the specification*/
static size_t hh_get_size_rstartreespec(void);
static size_t hh_serialize_rstartreespec(const RStarTreeSpecification *spec, uint8_t *buf);
static void hh_set_rstartreespec_from_serialization(RStarTreeSpecification *spec, uint8_t *buf, size_t *size);
/* these function serialize and read from serialization the HILBERTRTREE Specification from the header file
 but before, we get the size of the specification*/
static size_t hh_get_size_hilbertrtreespec(void);
static size_t hh_serialize_hilbertrtreespec(const HilbertRTreeSpecification *spec, uint8_t *buf);
static void hh_set_hilbertrtreespec_from_serialization(HilbertRTreeSpecification *spec, uint8_t *buf, size_t *size);
/* these function serialize and read from serialization the FAST Specification from the header file
 but before, we get the size of the specification*/
static size_t hh_get_size_fastspec(const FASTSpecification *spec);
static size_t hh_serialize_fastspec(const FASTSpecification *spec, uint8_t *buf);
static void hh_set_fastspec_from_serialization(FASTSpecification *spec, uint8_t *buf, size_t *size);
/* these function serialize and read from serialization the FORTree Specification from the header file
 but before, we get the size of the specification*/
static size_t hh_get_size_fortreespec(void);
static size_t hh_serialize_fortreespec(const FORTreeSpecification *spec, uint8_t *buf);
static void hh_set_fortreespec_from_serialization(FORTreeSpecification *spec, uint8_t *buf, size_t *size);
/* these function serialize and read from serialization the eFINDSpecification from the header file
 but before, we get the size of the specification*/
static size_t hh_get_size_efindspec(const eFINDSpecification *spec);
static size_t hh_serialize_efindspec(const eFINDSpecification *spec, uint8_t *buf);
static void hh_set_efindspec_from_serialization(eFINDSpecification *spec, uint8_t *buf, size_t *size);

/* R-TREE */
static void hh_write_rtree_header(const char *path, const RTree *r);
static SpatialIndex *hh_construct_rtree_from_header(const char *path);
/* R*-TREE */
static void hh_write_rstartree_header(const char *path, const RStarTree *r);
static SpatialIndex *hh_construct_rstartree_from_header(const char *path);
/* Hilbert R-TREE */
static void hh_write_hilbertrtree_header(const char *path, const HilbertRTree *r);
static SpatialIndex *hh_construct_hilbertrtree_from_header(const char *path);
/* FAST R-TREE */
static void hh_write_fastrtree_header(const char *path, const FASTRTree *r);
static SpatialIndex *hh_construct_fastrtree_from_header(const char *path);
/* FAST R*-TREE */
static void hh_write_fastrstartree_header(const char *path, const FASTRStarTree *r);
static SpatialIndex *hh_construct_fastrstartree_from_header(const char *path);
/* FAST Hilbert R-TREE */
static void hh_write_fasthilbertrtree_header(const char *path, const FASTHilbertRTree *r);
static SpatialIndex *hh_construct_fasthilbertrtree_from_header(const char *path);
/* FOR-TREE */
static void hh_write_fortree_header(const char *path, const FORTree *r);
static SpatialIndex *hh_construct_fortree_from_header(const char *path);
/* eFIND R-TREE */
static void hh_write_efindrtree_header(const char *path, const eFINDRTree *r);
static SpatialIndex *hh_construct_efindrtree_from_header(const char *path);
/* eFIND R*-TREE */
static void hh_write_efindrstartree_header(const char *path, const eFINDRStarTree *r);
static SpatialIndex *hh_construct_efindrstartree_from_header(const char *path);
/* eFIND Hilbert R-TREE */
static void hh_write_efindhilbertrtree_header(const char *path, const eFINDHilbertRTree *r);
static SpatialIndex *hh_construct_efindhilbertrtree_from_header(const char *path);

int hh_spc_open(const char *path) {
    int flag;
    int ret;

    /*we will use the normal access here since this file is very small*/
    flag = O_CREAT | O_RDWR;

    if ((ret = open(path, flag, S_IRUSR | S_IWUSR)) < 0) {
        _DEBUGF(ERROR, "It was impossible to open the \'%s\'. ", path);
        return -1;
    }
    return ret;
}

size_t hh_get_total_size(const char *path) {
    int file;
    size_t size;

    file = hh_spc_open(path);
    if (read(file, &size, sizeof (size_t)) != sizeof (size_t)) {
        _DEBUG(ERROR, "Problems to read the size of the header (8 bytes)!");
        return 0;
    }
    close(file);

    return size;
}

uint8_t hh_get_index_type(const char* path) {
    int file;
    uint8_t idx_type;

    file = hh_spc_open(path);
    if (lseek(file, sizeof (size_t), SEEK_SET) < 0) {
        _DEBUG(ERROR, "Error in lseek in get_index_type");
    }
    if (read(file, &idx_type, sizeof (uint8_t)) != sizeof (uint8_t)) {
        _DEBUG(ERROR, "Problems to read the index type (1 byte)!");
        return 0;
    }
    close(file);

    return idx_type;
}

size_t hh_get_size_source(const Source *src) {
    size_t ret = 0;

    ret += sizeof (int); //src_id

    ret += sizeof (uint32_t); //length of schema
    ret += strlen(src->schema) + 1; //size of schema

    ret += sizeof (uint32_t); //length of table
    ret += strlen(src->table) + 1; //size of table

    ret += sizeof (uint32_t); //length of column
    ret += strlen(src->column) + 1; //size of column

    ret += sizeof (uint32_t); //length of primary key
    ret += strlen(src->pk) + 1; //size of pk

    return ret;
}

size_t hh_serialize_source(const Source *src, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;

    int lcol = strlen(src->column) + 1;
    int ltab = strlen(src->table) + 1;
    int lsch = strlen(src->schema) + 1;
    int lpk = strlen(src->pk) + 1;

    /*src_id*/
    memcpy(loc, &(src->src_id), sizeof (int));
    loc += sizeof (int);

    /*schema*/
    memcpy(loc, &(lsch), sizeof (uint32_t));
    loc += sizeof (uint32_t);

    memcpy(loc, src->schema, (size_t) lsch);
    loc += lsch;

    /*table*/
    memcpy(loc, &(ltab), sizeof (uint32_t));
    loc += sizeof (uint32_t);

    memcpy(loc, src->table, (size_t) ltab);
    loc += ltab;

    /*geographic column*/
    memcpy(loc, &(lcol), sizeof (uint32_t));
    loc += sizeof (uint32_t);

    memcpy(loc, src->column, (size_t) lcol);
    loc += lcol;

    /*pk*/
    memcpy(loc, &(lpk), sizeof (uint32_t));
    loc += sizeof (uint32_t);

    memcpy(loc, src->pk, (size_t) lpk);
    loc += lpk;

    return_size = (size_t) (loc - buf);
    return return_size;
}

Source *hh_get_source_from_serialization(uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;
    Source *src = (Source*) lwalloc(sizeof (Source));

    int lcol, ltab, lsch, lpk;

    /*src_id*/
    memcpy(&src->src_id, buf, sizeof (int));
    buf += sizeof (int);

    /*schema*/
    memcpy(&(lsch), buf, sizeof (uint32_t));
    buf += sizeof (uint32_t);

    src->schema = (char*) lwalloc(lsch);

    memcpy(src->schema, buf, lsch);
    buf += lsch;

    /*table*/
    memcpy(&(ltab), buf, sizeof (uint32_t));
    buf += sizeof (uint32_t);

    src->table = (char*) lwalloc(ltab);

    memcpy(src->table, buf, ltab);
    buf += ltab;

    /*geographic columns*/
    memcpy(&(lcol), buf, sizeof (uint32_t));
    buf += sizeof (uint32_t);

    src->column = (char*) lwalloc(lcol);

    memcpy(src->column, buf, lcol);
    buf += lcol;

    /*pk*/
    memcpy(&(lpk), buf, sizeof (uint32_t));
    buf += sizeof (uint32_t);

    src->pk = (char*) lwalloc(lpk);

    memcpy(src->pk, buf, lpk);
    buf += lpk;

    if (size)
        *size = buf - start_ptr;

    return src;
}

size_t hh_get_size_storage_system(const StorageSystem *ss) {
    size_t ret = 0;

    ret += sizeof (int); //ss_id
    ret += sizeof (uint8_t); //type

    if (ss->type == FLASHDBSIM) {
        ret += (sizeof (int) * 13);
        /*there are 13 fields: nand_device_type, block_count, page_count_per_block, 
            page_size1, page_size2, erase_limitation, read_random_time, 
            read_serial_time, program_time, erase_time, 
            ftl_type, map_list_size, wear_leveling_threshold */
    }
    return ret;
}

size_t hh_serialize_storage_system(const StorageSystem *ss, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;

    /* ss id */
    memcpy(loc, &(ss->ss_id), sizeof (int));
    loc += sizeof (int);

    /* type */
    memcpy(loc, &(ss->type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    if (ss->type == FLASHDBSIM) {
        FlashDBSim *f = (FlashDBSim*) ss->info;
        /* nand_device_type */
        memcpy(loc, &(f->nand_device_type), sizeof (int));
        loc += sizeof (int);

        /* block_count */
        memcpy(loc, &(f->block_count), sizeof (int));
        loc += sizeof (int);

        /* page_count_per_block */
        memcpy(loc, &(f->page_count_per_block), sizeof (int));
        loc += sizeof (int);

        /* page_size1 */
        memcpy(loc, &(f->page_size1), sizeof (int));
        loc += sizeof (int);

        /* page_size2 */
        memcpy(loc, &(f->page_size2), sizeof (int));
        loc += sizeof (int);

        /* erase_limitation */
        memcpy(loc, &(f->erase_limitation), sizeof (int));
        loc += sizeof (int);

        /* read_random_time */
        memcpy(loc, &(f->read_random_time), sizeof (int));
        loc += sizeof (int);

        /* read_serial_time */
        memcpy(loc, &(f->read_serial_time), sizeof (int));
        loc += sizeof (int);

        /* program_time */
        memcpy(loc, &(f->program_time), sizeof (int));
        loc += sizeof (int);

        /* erase_time */
        memcpy(loc, &(f->erase_time), sizeof (int));
        loc += sizeof (int);

        /* ftl_type */
        memcpy(loc, &(f->ftl_type), sizeof (int));
        loc += sizeof (int);

        /* map_list_size */
        memcpy(loc, &(f->map_list_size), sizeof (int));
        loc += sizeof (int);

        /* wear_leveling_threshold */
        memcpy(loc, &(f->wear_leveling_threshold), sizeof (int));
        loc += sizeof (int);
    }

    return_size = (size_t) (loc - buf);
    return return_size;
}

StorageSystem *hh_get_storage_system_from_serialization(uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;
    StorageSystem *ss = (StorageSystem*) lwalloc(sizeof (StorageSystem));

    /* ss_id */
    memcpy(&(ss->ss_id), buf, sizeof (int));
    buf += sizeof (int);

    /* type */
    memcpy(&(ss->type), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    if (ss->type == FLASHDBSIM) {
        FlashDBSim *f = (FlashDBSim*) lwalloc(sizeof (FlashDBSim));
        /* nand_device_type */
        memcpy(&(f->nand_device_type), buf, sizeof (int));
        buf += sizeof (int);

        /* block_count */
        memcpy(&(f->block_count), buf, sizeof (int));
        buf += sizeof (int);

        /* page_count_per_block */
        memcpy(&(f->page_count_per_block), buf, sizeof (int));
        buf += sizeof (int);

        /* page_size1 */
        memcpy(&(f->page_size1), buf, sizeof (int));
        buf += sizeof (int);

        /* page_size2 */
        memcpy(&(f->page_size2), buf, sizeof (int));
        buf += sizeof (int);

        /* erase_limitation */
        memcpy(&(f->erase_limitation), buf, sizeof (int));
        buf += sizeof (int);

        /* read_random_time */
        memcpy(&(f->read_random_time), buf, sizeof (int));
        buf += sizeof (int);

        /* read_serial_time */
        memcpy(&(f->read_serial_time), buf, sizeof (int));
        buf += sizeof (int);

        /* program_time */
        memcpy(&(f->program_time), buf, sizeof (int));
        buf += sizeof (int);

        /* erase_time */
        memcpy(&(f->erase_time), buf, sizeof (int));
        buf += sizeof (int);

        /* ftl_type */
        memcpy(&(f->ftl_type), buf, sizeof (int));
        buf += sizeof (int);

        /* map_list_size */
        memcpy(&(f->map_list_size), buf, sizeof (int));
        buf += sizeof (int);

        /* wear_leveling_threshold */
        memcpy(&(f->wear_leveling_threshold), buf, sizeof (int));
        buf += sizeof (int);

        ss->info = (void*) f;

        /*check if the flashdbsim is already started*/
        check_flashsimulator_initialization(ss);
    }

    if (size)
        *size = buf - start_ptr;
    return ss;
}

size_t hh_get_size_generic_spec(const GenericParameters* gp) {
    size_t ret = 0;

    ret += hh_get_size_storage_system(gp->storage_system); //storage_system
    ret += sizeof (int); //bc_id
    ret += sizeof (uint8_t); //io_access
    ret += sizeof (int); //page_size
    ret += sizeof (uint8_t); //refinement_type

    return ret;
}

size_t hh_serialize_generic_spec(const GenericParameters* gp, uint8_t* buf) {
    uint8_t *loc = buf;
    size_t return_size;

    /* storage system */
    loc += hh_serialize_storage_system(gp->storage_system, loc);

    /* bc_id */
    memcpy(loc, &(gp->bc_id), sizeof (int));
    loc += sizeof (int);

    /* io access */
    memcpy(loc, &(gp->io_access), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* page size */
    memcpy(loc, &(gp->page_size), sizeof (int));
    loc += sizeof (int);

    /* refinement type */
    memcpy(loc, &(gp->refinement_type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    return_size = (size_t) (loc - buf);
    return return_size;
}

GenericParameters *hh_get_generic_spec_from_serialization(uint8_t* buf, size_t* size) {
    uint8_t *start_ptr = buf;
    size_t t_read;
    GenericParameters *gp = (GenericParameters*) lwalloc(sizeof (GenericParameters));

    /* storage system */
    gp->storage_system = hh_get_storage_system_from_serialization(buf, &t_read);
    buf += t_read;

    /* bc_id */
    memcpy(&(gp->bc_id), buf, sizeof (int));
    buf += sizeof (int);

    /* io access */
    memcpy(&(gp->io_access), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    /* page size */
    memcpy(&(gp->page_size), buf, sizeof (int));
    buf += sizeof (int);

    /* refinement type */
    memcpy(&(gp->refinement_type), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    if (size)
        *size = buf - start_ptr;

    return gp;
}

size_t hh_get_size_buffer_spec(const BufferSpecification *bs) {
    size_t ret = 0;

    ret += sizeof (int); //buf_id
    ret += sizeof (uint8_t); //buffer_type
    ret += sizeof (size_t); //min size
    ret += sizeof (size_t); //max size

    if (bs->buffer_type == BUFFER_S2Q) {
        ret += sizeof (size_t) + sizeof (size_t); //A1 and Am sizes
    } else if (bs->buffer_type == BUFFER_2Q) {
        ret += sizeof (size_t) + sizeof (size_t) + sizeof (size_t); //A1in, A1out, and Am sizes
    }

    return ret;
}

size_t hh_serialize_buffer_spec(const BufferSpecification *bs, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;

    /* buf_id */
    memcpy(loc, &(bs->buf_id), sizeof (int));
    loc += sizeof (int);

    /* buffer type */
    memcpy(loc, &(bs->buffer_type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* min size */
    memcpy(loc, &(bs->min_capacity), sizeof (size_t));
    loc += sizeof (size_t);

    /* max size */
    memcpy(loc, &(bs->max_capacity), sizeof (size_t));
    loc += sizeof (size_t);

    if (bs->buffer_type == BUFFER_S2Q) {
        BufferS2QSpecification *spec = (BufferS2QSpecification*) bs->buf_additional_param;

        /* A1 size */
        memcpy(loc, &(spec->A1_size), sizeof (size_t));
        loc += sizeof (size_t);

        /* Am size */
        memcpy(loc, &(spec->Am_size), sizeof (size_t));
        loc += sizeof (size_t);
    } else if (bs->buffer_type == BUFFER_2Q) {
        Buffer2QSpecification *spec = (Buffer2QSpecification*) bs->buf_additional_param;

        /* A1in size */
        memcpy(loc, &(spec->A1in_size), sizeof (size_t));
        loc += sizeof (size_t);

        /* A1out size */
        memcpy(loc, &(spec->A1out_size), sizeof (size_t));
        loc += sizeof (size_t);

        /* Am size */
        memcpy(loc, &(spec->Am_size), sizeof (size_t));
        loc += sizeof (size_t);
    }

    return_size = (size_t) (loc - buf);
    return return_size;
}

BufferSpecification *hh_get_buffer_spec_from_serialization(uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;
    BufferSpecification *bs = (BufferSpecification*) lwalloc(sizeof (BufferSpecification));

    /* buf_id */
    memcpy(&(bs->buf_id), buf, sizeof (int));
    buf += sizeof (int);

    /* buffer type */
    memcpy(&(bs->buffer_type), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    /* min size */
    memcpy(&(bs->min_capacity), buf, sizeof (size_t));
    buf += sizeof (size_t);

    /* max size */
    memcpy(&(bs->max_capacity), buf, sizeof (size_t));
    buf += sizeof (size_t);

    if (bs->buffer_type == BUFFER_S2Q) {
        BufferS2QSpecification *spec = (BufferS2QSpecification*) lwalloc(sizeof (BufferS2QSpecification));

        /* A1 size */
        memcpy(&(spec->A1_size), buf, sizeof (size_t));
        buf += sizeof (size_t);

        /* Am size */
        memcpy(&(spec->Am_size), buf, sizeof (size_t));
        buf += sizeof (size_t);

        bs->buf_additional_param = (void*) spec;
    } else if (bs->buffer_type == BUFFER_2Q) {
        Buffer2QSpecification *spec = (Buffer2QSpecification*) lwalloc(sizeof (Buffer2QSpecification));

        /* A1in size */
        memcpy(&(spec->A1in_size), buf, sizeof (size_t));
        buf += sizeof (size_t);

        /* A1out size */
        memcpy(&(spec->A1out_size), buf, sizeof (size_t));
        buf += sizeof (size_t);

        /* Am size */
        memcpy(&(spec->Am_size), buf, sizeof (size_t));
        buf += sizeof (size_t);

        bs->buf_additional_param = (void*) spec;
    }

    if (size)
        *size = buf - start_ptr;

    return bs;
}

size_t hh_get_size_other_param() {
    return sizeof (int); //sc_id
}

size_t hh_serialize_other_param(const SpatialIndex *si, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;

    /* sc_id */
    memcpy(loc, &(si->sc_id), sizeof (int));
    loc += sizeof (int);

    return_size = (size_t) (loc - buf);
    return return_size;
}

int hh_get_sc_id(uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;
    int sc_id;

    /* sc_id */
    memcpy(&(sc_id), buf, sizeof (int));
    buf += sizeof (int);

    if (size)
        *size = buf - start_ptr;

    return sc_id;
}

size_t hh_get_size_rtrees_info(const RTreesInfo *info) {
    size_t ret = 0;

    ret += sizeof (int); //root_page
    ret += sizeof (int); //height
    ret += sizeof (int); //last_allocated_page    
    ret += sizeof (int); //number of empty pages
    ret += sizeof (int) * info->nof_empty_pages; //empty pages      

    return ret;
}

size_t hh_serialize_rtrees_info(const RTreesInfo *info, uint8_t *buf) {
    int i;
    uint8_t *loc = buf;
    size_t return_size;

    /* root page */
    memcpy(loc, &(info->root_page), sizeof (int));
    loc += sizeof (int);

    /* height of the tree */
    memcpy(loc, &(info->height), sizeof (int));
    loc += sizeof (int);

    /* last allocated page */
    memcpy(loc, &(info->last_allocated_page), sizeof (int));
    loc += sizeof (int);

    /* number of empty pages */
    memcpy(loc, &(info->nof_empty_pages), sizeof (int));
    loc += sizeof (int);

    /* empty pages */
    for (i = 0; i < info->nof_empty_pages; i++) {
        memcpy(loc, &(info->empty_pages[i]), sizeof (int));
        loc += sizeof (int);
    }

    return_size = (size_t) (loc - buf);
    return return_size;
}

void hh_set_rtrees_info_from_serialization(RTreesInfo *info, uint8_t *buf, size_t *size) {
    int *empty;
    uint8_t *start_ptr = buf;

    /* root page */
    memcpy(&(info->root_page), buf, sizeof (int));
    buf += sizeof (int);

    /* height of the tree */
    memcpy(&(info->height), buf, sizeof (int));
    buf += sizeof (int);

    /* last allocated page */
    memcpy(&(info->last_allocated_page), buf, sizeof (int));
    buf += sizeof (int);

    /* number of empty pages */
    memcpy(&(info->nof_empty_pages), buf, sizeof (int));
    buf += sizeof (int);

    /* empty pages */
    empty = (int*) lwalloc(sizeof (int) * info->nof_empty_pages);
    memcpy(empty, buf, sizeof (int) * info->nof_empty_pages);
    buf += sizeof (int) * info->nof_empty_pages;

    rtreesinfo_set_empty_pages(info, empty, info->nof_empty_pages, info->nof_empty_pages);

    if (size)
        *size = buf - start_ptr;
}

size_t hh_get_size_rtreespec(void) {
    size_t ret = 0;
    /* or_id */
    ret += sizeof (int);
    ret += sizeof (int); //max_entries_int_node
    ret += sizeof (int); //max_entries_leaf_node
    ret += sizeof (int); //min_entries_int_node    
    ret += sizeof (int); //min_entries_leaf_node
    ret += sizeof (uint8_t); //split

    return ret;
}

size_t hh_serialize_rtreespec(const RTreeSpecification *spec, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;

    /* or_id */
    memcpy(loc, &(spec->or_id), sizeof (int));
    loc += sizeof (int);

    /* max_entries_int_node */
    memcpy(loc, &(spec->max_entries_int_node), sizeof (int));
    loc += sizeof (int);

    /* max_entries_leaf_node */
    memcpy(loc, &(spec->max_entries_leaf_node), sizeof (int));
    loc += sizeof (int);

    /* min_entries_int_node */
    memcpy(loc, &(spec->min_entries_int_node), sizeof (int));
    loc += sizeof (int);

    /* min_entries_leaf_node */
    memcpy(loc, &(spec->min_entries_leaf_node), sizeof (int));
    loc += sizeof (int);

    /*split*/
    memcpy(loc, &(spec->split_type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    return_size = (size_t) (loc - buf);
    return return_size;
}

void hh_set_rtreespec_from_serialization(RTreeSpecification *spec, uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;

    /* or_id */
    memcpy(&(spec->or_id), buf, sizeof (int));
    buf += sizeof (int);

    /* max_entries_int_node */
    memcpy(&(spec->max_entries_int_node), buf, sizeof (int));
    buf += sizeof (int);

    /* max_entries_leaf_node */
    memcpy(&(spec->max_entries_leaf_node), buf, sizeof (int));
    buf += sizeof (int);

    /* min_entries_int_node */
    memcpy(&(spec->min_entries_int_node), buf, sizeof (int));
    buf += sizeof (int);

    /* min_entries_leaf_node */
    memcpy(&(spec->min_entries_leaf_node), buf, sizeof (int));
    buf += sizeof (int);

    /*split*/
    memcpy(&(spec->split_type), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    if (size)
        *size = buf - start_ptr;
}

size_t hh_get_size_rstartreespec() {
    size_t ret = 0;

    /* or_id */
    ret += sizeof (int);
    ret += sizeof (int); //max_entries_int_node
    ret += sizeof (int); //max_entries_leaf_node
    ret += sizeof (int); //min_entries_int_node    
    ret += sizeof (int); //min_entries_leaf_node
    ret += sizeof (double); //reinsert_perc_internal_node
    ret += sizeof (double); //reinsert_perc_leaf_node
    ret += sizeof (uint8_t); //reinsertion type
    ret += sizeof (int); //max_neighbors_to_examine

    return ret;
}

size_t hh_serialize_rstartreespec(const RStarTreeSpecification *spec, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;

    /* or_id */
    memcpy(loc, &(spec->or_id), sizeof (int));
    loc += sizeof (int);

    /* max_entries_int_node */
    memcpy(loc, &(spec->max_entries_int_node), sizeof (int));
    loc += sizeof (int);

    /* max_entries_leaf_node */
    memcpy(loc, &(spec->max_entries_leaf_node), sizeof (int));
    loc += sizeof (int);

    /* min_entries_int_node */
    memcpy(loc, &(spec->min_entries_int_node), sizeof (int));
    loc += sizeof (int);

    /* min_entries_leaf_node */
    memcpy(loc, &(spec->min_entries_leaf_node), sizeof (int));
    loc += sizeof (int);

    /* reinsert_perc_internal_node */
    memcpy(loc, &(spec->reinsert_perc_internal_node), sizeof (double));
    loc += sizeof (double);

    /* reinsert_perc_leaf_node */
    memcpy(loc, &(spec->reinsert_perc_leaf_node), sizeof (double));
    loc += sizeof (double);

    /* reinsertion type */
    memcpy(loc, &(spec->reinsert_type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* max_neighbors_to_examine */
    memcpy(loc, &(spec->max_neighbors_to_examine), sizeof (int));
    loc += sizeof (int);

    return_size = (size_t) (loc - buf);
    return return_size;
}

void hh_set_rstartreespec_from_serialization(RStarTreeSpecification *spec, uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;

    /* or_id */
    memcpy(&(spec->or_id), buf, sizeof (int));
    buf += sizeof (int);

    /* max_entries_int_node */
    memcpy(&(spec->max_entries_int_node), buf, sizeof (int));
    buf += sizeof (int);

    /* max_entries_leaf_node */
    memcpy(&(spec->max_entries_leaf_node), buf, sizeof (int));
    buf += sizeof (int);

    /* min_entries_int_node */
    memcpy(&(spec->min_entries_int_node), buf, sizeof (int));
    buf += sizeof (int);

    /* min_entries_leaf_node */
    memcpy(&(spec->min_entries_leaf_node), buf, sizeof (int));
    buf += sizeof (int);

    /* reinsert_perc_internal_node */
    memcpy(&(spec->reinsert_perc_internal_node), buf, sizeof (double));
    buf += sizeof (double);

    /* reinsert_perc_leaf_node */
    memcpy(&(spec->reinsert_perc_leaf_node), buf, sizeof (double));
    buf += sizeof (double);

    /* reinsertion type */
    memcpy(&(spec->reinsert_type), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    /* max_neighbors_to_examine */
    memcpy(&(spec->max_neighbors_to_examine), buf, sizeof (int));
    buf += sizeof (int);

    if (size)
        *size = buf - start_ptr;
}

size_t hh_get_size_hilbertrtreespec(void) {
    size_t ret = 0;
    /* or_id */
    ret += sizeof (int);
    ret += sizeof (int); //max_entries_int_node
    ret += sizeof (int); //max_entries_leaf_node
    ret += sizeof (int); //min_entries_int_node    
    ret += sizeof (int); //min_entries_leaf_node
    ret += sizeof (int); //order_splitting_policy
    ret += sizeof (int); //srid

    return ret;
}

size_t hh_serialize_hilbertrtreespec(const HilbertRTreeSpecification *spec, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;

    /* or_id */
    memcpy(loc, &(spec->or_id), sizeof (int));
    loc += sizeof (int);

    /* max_entries_int_node */
    memcpy(loc, &(spec->max_entries_int_node), sizeof (int));
    loc += sizeof (int);

    /* max_entries_leaf_node */
    memcpy(loc, &(spec->max_entries_leaf_node), sizeof (int));
    loc += sizeof (int);

    /* min_entries_int_node */
    memcpy(loc, &(spec->min_entries_int_node), sizeof (int));
    loc += sizeof (int);

    /* min_entries_leaf_node */
    memcpy(loc, &(spec->min_entries_leaf_node), sizeof (int));
    loc += sizeof (int);

    /*order_splitting_policy*/
    memcpy(loc, &(spec->order_splitting_policy), sizeof (int));
    loc += sizeof (int);

    /*srid*/
    memcpy(loc, &(spec->srid), sizeof (int));
    loc += sizeof (int);

    return_size = (size_t) (loc - buf);
    return return_size;
}

void hh_set_hilbertrtreespec_from_serialization(HilbertRTreeSpecification *spec, uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;

    /* or_id */
    memcpy(&(spec->or_id), buf, sizeof (int));
    buf += sizeof (int);

    /* max_entries_int_node */
    memcpy(&(spec->max_entries_int_node), buf, sizeof (int));
    buf += sizeof (int);

    /* max_entries_leaf_node */
    memcpy(&(spec->max_entries_leaf_node), buf, sizeof (int));
    buf += sizeof (int);

    /* min_entries_int_node */
    memcpy(&(spec->min_entries_int_node), buf, sizeof (int));
    buf += sizeof (int);

    /* min_entries_leaf_node */
    memcpy(&(spec->min_entries_leaf_node), buf, sizeof (int));
    buf += sizeof (int);

    /*order_splitting_policy*/
    memcpy(&(spec->order_splitting_policy), buf, sizeof (int));
    buf += sizeof (int);

    /*srid*/
    memcpy(&(spec->srid), buf, sizeof (int));
    buf += sizeof (int);

    if (size)
        *size = buf - start_ptr;
}

size_t hh_get_size_fastspec(const FASTSpecification *spec) {
    size_t ret = 0;

    ret += sizeof (size_t); //buffer_size
    ret += sizeof (int); //flushing_unit_size
    ret += sizeof (uint8_t); //flushing_policy    
    ret += sizeof (size_t); //log_size
    ret += sizeof (int); //length of the log_file
    ret += strlen(spec->log_file) + 1; //log_file
    ret += sizeof (int); //index_sc_id
    ret += sizeof (size_t); //offset_last_elem_log
    ret += sizeof (size_t); //size_last_elem_log

    return ret;
}

size_t hh_serialize_fastspec(const FASTSpecification *spec, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;
    int llfile = strlen(spec->log_file) + 1;

    /* buffer_size */
    memcpy(loc, &(spec->buffer_size), sizeof (size_t));
    loc += sizeof (size_t);

    /* flushing_unit_size */
    memcpy(loc, &(spec->flushing_unit_size), sizeof (int));
    loc += sizeof (int);

    /* flushing_policy */
    memcpy(loc, &(spec->flushing_policy), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* log_size */
    memcpy(loc, &(spec->log_size), sizeof (size_t));
    loc += sizeof (size_t);

    /* length of the log file */
    memcpy(loc, &(llfile), sizeof (int));
    loc += sizeof (int);

    /* log file */
    memcpy(loc, spec->log_file, (size_t) llfile);
    loc += llfile;

    /* index_sc_id */
    memcpy(loc, &(spec->index_sc_id), sizeof (int));
    loc += sizeof (int);

    /* offset_last_elem_log */
    memcpy(loc, &(spec->offset_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    /* size_last_elem_log */
    memcpy(loc, &(spec->size_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    return_size = (size_t) (loc - buf);
    return return_size;
}

void hh_set_fastspec_from_serialization(FASTSpecification *spec, uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;

    int llfile = 0;

    /* buffer_size */
    memcpy(&(spec->buffer_size), buf, sizeof (size_t));
    buf += sizeof (size_t);

    /* flushing_unit_size */
    memcpy(&(spec->flushing_unit_size), buf, sizeof (int));
    buf += sizeof (int);

    /* flushing_policy */
    memcpy(&(spec->flushing_policy), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    /* log_size */
    memcpy(&(spec->log_size), buf, sizeof (size_t));
    buf += sizeof (size_t);

    /* length of the log file */
    memcpy(&(llfile), buf, sizeof (int));
    buf += sizeof (int);

    spec->log_file = (char*) lwalloc(llfile);

    /* log file */
    memcpy(spec->log_file, buf, (size_t) llfile);
    buf += llfile;

    /* index_sc_id */
    memcpy(&(spec->index_sc_id), buf, sizeof (int));
    buf += sizeof (int);

    /* offset_last_elem_log */
    memcpy(&(spec->offset_last_elem_log), buf, sizeof (size_t));
    buf += sizeof (size_t);

    /* size_last_elem_log */
    memcpy(&(spec->size_last_elem_log), buf, sizeof (size_t));
    buf += sizeof (size_t);

    if (size)
        *size = buf - start_ptr;
}

size_t hh_get_size_fortreespec(void) {
    size_t ret = 0;

    /* or_id */
    ret += sizeof (int);
    ret += sizeof (int); //max_entries_int_node
    ret += sizeof (int); //max_entries_leaf_node
    ret += sizeof (int); //min_entries_int_node
    ret += sizeof (int); //min_entries_leaf_node
    ret += sizeof (size_t); //buffer_size
    ret += sizeof (int); //flushing_unit_size
    ret += sizeof (double); //ratio_flushing    
    ret += sizeof (double); //x
    ret += sizeof (double); //y

    return ret;
}

size_t hh_serialize_fortreespec(const FORTreeSpecification *spec, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;

    /* or_id */
    memcpy(loc, &(spec->or_id), sizeof (int));
    loc += sizeof (int);

    /* max_entries_int_node */
    memcpy(loc, &(spec->max_entries_int_node), sizeof (int));
    loc += sizeof (int);

    /* max_entries_leaf_node */
    memcpy(loc, &(spec->max_entries_leaf_node), sizeof (int));
    loc += sizeof (int);

    /* min_entries_int_node */
    memcpy(loc, &(spec->min_entries_int_node), sizeof (int));
    loc += sizeof (int);

    /* min_entries_leaf_node */
    memcpy(loc, &(spec->min_entries_leaf_node), sizeof (int));
    loc += sizeof (int);

    /* buffer_size */
    memcpy(loc, &(spec->buffer_size), sizeof (size_t));
    loc += sizeof (size_t);

    /* flushing_unit_size */
    memcpy(loc, &(spec->flushing_unit_size), sizeof (int));
    loc += sizeof (int);

    /* ratio_flushing */
    memcpy(loc, &(spec->ratio_flushing), sizeof (double));
    loc += sizeof (double);

    /* x */
    memcpy(loc, &(spec->x), sizeof (double));
    loc += sizeof (double);

    /* y */
    memcpy(loc, &(spec->y), sizeof (double));
    loc += sizeof (double);

    return_size = (size_t) (loc - buf);
    return return_size;
}

void hh_set_fortreespec_from_serialization(FORTreeSpecification *spec, uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;

    /* or_id */
    memcpy(&(spec->or_id), buf, sizeof (int));
    buf += sizeof (int);

    /* max_entries_int_node */
    memcpy(&(spec->max_entries_int_node), buf, sizeof (int));
    buf += sizeof (int);

    /* max_entries_leaf_node */
    memcpy(&(spec->max_entries_leaf_node), buf, sizeof (int));
    buf += sizeof (int);

    /* min_entries_int_node */
    memcpy(&(spec->min_entries_int_node), buf, sizeof (int));
    buf += sizeof (int);

    /* min_entries_leaf_node */
    memcpy(&(spec->min_entries_leaf_node), buf, sizeof (int));
    buf += sizeof (int);

    /* buffer_size */
    memcpy(&(spec->buffer_size), buf, sizeof (size_t));
    buf += sizeof (size_t);

    /* flushing_unit_size */
    memcpy(&(spec->flushing_unit_size), buf, sizeof (int));
    buf += sizeof (int);

    /* ratio_flushing */
    memcpy(&(spec->ratio_flushing), buf, sizeof (double));
    buf += sizeof (double);

    /* x */
    memcpy(&(spec->x), buf, sizeof (double));
    buf += sizeof (double);

    /* y */
    memcpy(&(spec->y), buf, sizeof (double));
    buf += sizeof (double);

    if (size)
        *size = buf - start_ptr;
}

size_t hh_get_size_efindspec(const eFINDSpecification *spec) {
    size_t ret = 0;

    ret += sizeof (size_t); //buffer_size for write buffer
    ret += sizeof (size_t); //buffer_size for read buffer
    ret += sizeof (double); //buffer read size perc

    ret += sizeof (uint8_t); //read_buffer_policy 

    //the additional parameter
    if (spec->read_buffer_policy == eFIND_2Q_RBP) {
        ret += sizeof (double); //A1in_perc_size
    }

    ret += sizeof (uint8_t); //temporal_control_policy
    ret += sizeof (double); //read temporal control percentage

    ret += sizeof (int); //size of temporal control for writes
    ret += sizeof (int); //temporal control for writes the minimum distance to be considered
    ret += sizeof (int); //temporal control for write the width of the stride

    ret += sizeof (double); //percentage value to be in the flushing operation, based on the timestamp

    ret += sizeof (int); //flushing_unit_size
    ret += sizeof (uint8_t); //flushing_policy    

    ret += sizeof (size_t); //log_size
    ret += sizeof (int); //length of the log_file
    ret += strlen(spec->log_file) + 1; //log_file
    ret += sizeof (int); //index_sc_id
    ret += sizeof (size_t); //offset_last_elem_log
    ret += sizeof (size_t); //size_last_elem_log

    return ret;
}

size_t hh_serialize_efindspec(const eFINDSpecification *spec, uint8_t *buf) {
    uint8_t *loc = buf;
    size_t return_size;
    int llfile = strlen(spec->log_file) + 1;

    /* write_buffer_size */
    memcpy(loc, &(spec->write_buffer_size), sizeof (size_t));
    loc += sizeof (size_t);
    /* read_buffer_size */
    memcpy(loc, &(spec->read_buffer_size), sizeof (size_t));
    loc += sizeof (size_t);

    /* read_buffer_perc */
    memcpy(loc, &(spec->read_buffer_perc), sizeof (double));
    loc += sizeof (double);

    /* read_buffer_policy */
    memcpy(loc, &(spec->read_buffer_policy), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    if (spec->read_buffer_policy == eFIND_2Q_RBP) {
        eFIND2QSpecification *spec_2q = (eFIND2QSpecification*) spec->rbp_additional_params;

        /* we should serialize the additional parameter of the 2Q here*/
        memcpy(loc, &(spec_2q->A1in_perc_size), sizeof (double));
        loc += sizeof (double);
    }

    /* temporal_control_policy */
    memcpy(loc, &(spec->temporal_control_policy), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* read_temporal_control_size */
    memcpy(loc, &(spec->read_temporal_control_perc), sizeof (double));
    loc += sizeof (double);

    /* write_temporal_control_size */
    memcpy(loc, &(spec->write_temporal_control_size), sizeof (int));
    loc += sizeof (int);

    /* write_tc_minimum_distance */
    memcpy(loc, &(spec->write_tc_minimum_distance), sizeof (int));
    loc += sizeof (int);

    /* write_tc_stride */
    memcpy(loc, &(spec->write_tc_stride), sizeof (int));
    loc += sizeof (int);

    /* sort_perc */
    memcpy(loc, &(spec->timestamp_perc), sizeof (double));
    loc += sizeof (double);

    /* flushing_unit_size */
    memcpy(loc, &(spec->flushing_unit_size), sizeof (int));
    loc += sizeof (int);

    /* flushing_policy */
    memcpy(loc, &(spec->flushing_policy), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* log_size */
    memcpy(loc, &(spec->log_size), sizeof (size_t));
    loc += sizeof (size_t);

    /* length of the log file */
    memcpy(loc, &(llfile), sizeof (int));
    loc += sizeof (int);

    /* log file */
    memcpy(loc, spec->log_file, (size_t) llfile);
    loc += llfile;

    /* index_sc_id */
    memcpy(loc, &(spec->index_sc_id), sizeof (int));
    loc += sizeof (int);

    /* offset_last_elem_log */
    memcpy(loc, &(spec->offset_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    /* size_last_elem_log */
    memcpy(loc, &(spec->size_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    return_size = (size_t) (loc - buf);
    return return_size;
}

void hh_set_efindspec_from_serialization(eFINDSpecification *spec, uint8_t *buf, size_t *size) {
    uint8_t *start_ptr = buf;

    int llfile = 0;

    /* write_buffer_size */
    memcpy(&(spec->write_buffer_size), buf, sizeof (size_t));
    buf += sizeof (size_t);
    /* read_buffer_size */
    memcpy(&(spec->read_buffer_size), buf, sizeof (size_t));
    buf += sizeof (size_t);

    /* read_buffer_perc */
    memcpy(&(spec->read_buffer_perc), buf, sizeof (double));
    buf += sizeof (double);

    /* read_buffer_policy */
    memcpy(&(spec->read_buffer_policy), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    if (spec->read_buffer_policy == eFIND_2Q_RBP) {
        eFIND2QSpecification *spec_2q = (eFIND2QSpecification*)
                lwalloc(sizeof (eFIND2QSpecification));

        /* we should read the additional parameter of the 2Q here*/
        memcpy(&(spec_2q->A1in_perc_size), buf, sizeof (double));
        buf += sizeof (double);

        spec->rbp_additional_params = (void*) spec_2q;
    }

    /* temporal_control_policy */
    memcpy(&(spec->temporal_control_policy), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    /* read_temporal_control_size */
    memcpy(&(spec->read_temporal_control_perc), buf, sizeof (double));
    buf += sizeof (double);

    /* write_temporal_control_size */
    memcpy(&(spec->write_temporal_control_size), buf, sizeof (int));
    buf += sizeof (int);

    /* write_tc_minimum_distance */
    memcpy(&(spec->write_tc_minimum_distance), buf, sizeof (int));
    buf += sizeof (int);

    /* write_tc_stride */
    memcpy(&(spec->write_tc_stride), buf, sizeof (int));
    buf += sizeof (int);

    /* sort_perc */
    memcpy(&(spec->timestamp_perc), buf, sizeof (double));
    buf += sizeof (double);

    /* flushing_unit_size */
    memcpy(&(spec->flushing_unit_size), buf, sizeof (int));
    buf += sizeof (int);

    /* flushing_policy */
    memcpy(&(spec->flushing_policy), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    /* log_size */
    memcpy(&(spec->log_size), buf, sizeof (size_t));
    buf += sizeof (size_t);

    /* length of the log file */
    memcpy(&(llfile), buf, sizeof (int));
    buf += sizeof (int);

    spec->log_file = (char*) lwalloc(llfile);

    /* log file */
    memcpy(spec->log_file, buf, (size_t) llfile);
    buf += llfile;

    /* index_sc_id */
    memcpy(&(spec->index_sc_id), buf, sizeof (int));
    buf += sizeof (int);

    /* offset_last_elem_log */
    memcpy(&(spec->offset_last_elem_log), buf, sizeof (size_t));
    buf += sizeof (size_t);

    /* size_last_elem_log */
    memcpy(&(spec->size_last_elem_log), buf, sizeof (size_t));
    buf += sizeof (size_t);

    if (size)
        *size = buf - start_ptr;
}

/* R-TREE */
void hh_write_rtree_header(const char *path, const RTree *r) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_rtreespec();

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(r->type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rtreespec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_rtreespec(r->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_rtree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_rtree!", written, bufsize);
        return;
    }
    close(file);

    lwfree(buf);
}

SpatialIndex *hh_construct_rtree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    RTree *rtree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_rtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    //then we read the source
    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the generic parameters
    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = rtree_empty_create(index_path, src, gp, bs, false);

    //the we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    rtree = (void *) si;

    hh_set_rtrees_info_from_serialization(rtree->info, loc, &t_read);
    loc += t_read;

    //_DEBUGF(NOTICE, "%d, %d, %d, %d", rtree->info->root_page, rtree->info->height,
    //        rtree->info->last_allocated_page, rtree->info->nof_empty_pages);

    hh_set_rtreespec_from_serialization(rtree->spec, loc, &t_read);
    loc += t_read;

    //_DEBUGF(NOTICE, "%d, %d, %d, %d, %d, %d", rtree->spec->max_entries_int_node, rtree->spec->max_entries_leaf_node,
    //        rtree->spec->min_entries_int_node, rtree->spec->min_entries_leaf_node,
    //       rtree->spec->or_id, rtree->spec->split_type);

    //now we have to read the root node
    rtree->current_node = get_rnode(si, rtree->info->root_page, rtree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (rtree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(rtree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

/* R*-TREE */
void hh_write_rstartree_header(const char *path, const RStarTree *r) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_rstartreespec();

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(r->type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rstartreespec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_rstartreespec(r->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_rstartree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_rstartree!", written, bufsize);
        return;
    }
    close(file);

    lwfree(buf);
}

SpatialIndex *hh_construct_rstartree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    RStarTree *rstartree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_rtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    //then we read the generic_tree_specialization
    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = rstartree_empty_create(index_path, src, gp, bs, false);

    //the we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    rstartree = (void *) si;

    hh_set_rtrees_info_from_serialization(rstartree->info, loc, &t_read);
    loc += t_read;

    //_DEBUGF(NOTICE, "%d, %d, %d, %d", rstartree->info->root_page, rstartree->info->height,
    //        rstartree->info->last_allocated_page, rstartree->info->nof_empty_pages);

    hh_set_rstartreespec_from_serialization(rstartree->spec, loc, &t_read);
    loc += t_read;

    //_DEBUGF(NOTICE, "%d, %d, %d, %d, %d, %d %f %f", rstartree->spec->max_entries_int_node, rstartree->spec->max_entries_leaf_node,
    //        rstartree->spec->min_entries_int_node, rstartree->spec->min_entries_leaf_node,
    //        rstartree->spec->or_id, rstartree->spec->reinsert_type,
    //        rstartree->spec->reinsert_perc_leaf_node, rstartree->spec->reinsert_perc_internal_node);

    //now we have to read the root node
    rstartree->current_node = get_rnode(si, rstartree->info->root_page, rstartree->info->height);

    //if we have a rstartree with height greater than 1
    if (rstartree->info->height >= 1) {
        int i;
        rstartree->reinsert = (bool*) lwrealloc(rstartree->reinsert,
                sizeof (bool) * (rstartree->info->height + 1));
        for (i = 0; i < rstartree->info->height; i++)
            rstartree->reinsert[i] = true;
        rstartree->reinsert[rstartree->info->height] = false;
    }

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (rstartree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(rstartree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

void hh_write_hilbertrtree_header(const char *path, const HilbertRTree *r) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_hilbertrtreespec();

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(r->type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rtreespec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_hilbertrtreespec(r->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_rtree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_rtree!", written, bufsize);
        return;
    }
    close(file);

    lwfree(buf);
}

SpatialIndex *hh_construct_hilbertrtree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    HilbertRTree *hrtree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_hilbertrtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    //then we read the source
    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the generic parameters
    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = hilbertrtree_empty_create(index_path, src, gp, bs, false);

    //the we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    hrtree = (void *) si;

    hh_set_rtrees_info_from_serialization(hrtree->info, loc, &t_read);
    loc += t_read;

    hh_set_hilbertrtreespec_from_serialization(hrtree->spec, loc, &t_read);
    loc += t_read;

    //_DEBUGF(NOTICE, "%d, %d, %d, %d, %d, %d", hrtree->spec->max_entries_int_node, hrtree->spec->max_entries_leaf_node,
    //        hrtree->spec->min_entries_int_node, hrtree->spec->min_entries_leaf_node,
    //       hrtree->spec->or_id, hrtree->spec->order_splitting_policy);

    //now we have to read the root node
    hrtree->current_node = get_hilbertnode(si, hrtree->info->root_page, hrtree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (hrtree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(hrtree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

/* FAST R-tree */
void hh_write_fastrtree_header(const char *path, const FASTRTree *fastr) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;
    RTree *r;
    uint8_t type = FAST_RTREE_TYPE;

    r = fastr->rtree;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_rtreespec();
    bufsize += hh_get_size_fastspec(fastr->spec);

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rtreespec, fastspec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_rtreespec(r->spec, loc);
    loc += hh_serialize_fastspec(fastr->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_fastrtree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_fastrtree!", written, bufsize);
        return;
    }
    close(file);

    lwfree(buf);
}

SpatialIndex *hh_construct_fastrtree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    FASTIndex *fi;
    FASTRTree *fastrtree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_rtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = fastrtree_empty_create(index_path, src, gp, bs, NULL, false);

    //the we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    fi = (void *) si;
    fastrtree = fi->fast_index.fast_rtree;
    fastrtree->rtree->base.sc_id = si->sc_id;
    fastrtree->spec = (FASTSpecification*) lwalloc(sizeof (FASTSpecification));

    hh_set_rtrees_info_from_serialization(fastrtree->rtree->info, loc, &t_read);
    loc += t_read;

    hh_set_rtreespec_from_serialization(fastrtree->rtree->spec, loc, &t_read);
    loc += t_read;

    hh_set_fastspec_from_serialization(fastrtree->spec, loc, &t_read);
    loc += t_read;

    //_DEBUGF(NOTICE, "%d, %d, %d, %d", fastrtree->rtree->info->root_page, fastrtree->rtree->info->height,
    //       fastrtree->rtree->info->last_allocated_page, fastrtree->rtree->info->nof_empty_pages);

    //_DEBUGF(NOTICE, "%d, %d, %d, %d, %d", fastrtree->rtree->spec->max_entries_int_node, fastrtree->rtree->spec->max_entries_leaf_node,
    //        fastrtree->rtree->spec->min_entries_int_node, fastrtree->rtree->spec->min_entries_leaf_node,
    //        fastrtree->rtree->spec->or_id);

    //_DEBUGF(NOTICE, "%d, %d, %d, %d, %s, %d, %d, %d", fastrtree->spec->buffer_size, fastrtree->spec->flushing_policy,
    //        fastrtree->spec->flushing_unit_size, fastrtree->spec->index_sc_id, fastrtree->spec->log_file,
    //        fastrtree->spec->log_size, fastrtree->spec->offset_last_elem_log, fastrtree->spec->size_last_elem_log);

    //now we have to read the root node
    fastrtree->rtree->current_node = (RNode *) fb_retrieve_node(&fi->base,
            fastrtree->rtree->info->root_page, fastrtree->rtree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (fastrtree->rtree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(fastrtree->rtree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

/* FAST R*-TREE */
void hh_write_fastrstartree_header(const char *path, const FASTRStarTree *fastr) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;
    RStarTree *r;
    uint8_t type = FAST_RSTARTREE_TYPE;

    r = fastr->rstartree;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_rstartreespec();
    bufsize += hh_get_size_fastspec(fastr->spec);

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rstartreespec, fastspec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_rstartreespec(r->spec, loc);
    loc += hh_serialize_fastspec(fastr->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_fastrstartree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_fastrstartree!", written, bufsize);
        return;
    }
    close(file);

    lwfree(buf);
}

SpatialIndex *hh_construct_fastrstartree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    FASTIndex *fi;
    FASTRStarTree *fastrstartree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_rtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = fastrstartree_empty_create(index_path, src, gp, bs, NULL, false);

    //the we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    fi = (void *) si;
    fastrstartree = fi->fast_index.fast_rstartree;
    fastrstartree->rstartree->base.sc_id = si->sc_id;
    fastrstartree->spec = (FASTSpecification*) lwalloc(sizeof (FASTSpecification));

    hh_set_rtrees_info_from_serialization(fastrstartree->rstartree->info, loc, &t_read);
    loc += t_read;

    hh_set_rstartreespec_from_serialization(fastrstartree->rstartree->spec, loc, &t_read);
    loc += t_read;

    hh_set_fastspec_from_serialization(fastrstartree->spec, loc, &t_read);
    loc += t_read;

    //now we have to read the root node
    fastrstartree->rstartree->current_node = (RNode *) fb_retrieve_node(&fi->base,
            fastrstartree->rstartree->info->root_page, fastrstartree->rstartree->info->height);

    //if we have a rstartree with height greater than 1
    if (fastrstartree->rstartree->info->height >= 1) {
        int i;
        fastrstartree->rstartree->reinsert = (bool*) lwrealloc(fastrstartree->rstartree->reinsert,
                sizeof (bool) * (fastrstartree->rstartree->info->height + 1));
        for (i = 0; i < fastrstartree->rstartree->info->height; i++)
            fastrstartree->rstartree->reinsert[i] = true;
        fastrstartree->rstartree->reinsert[fastrstartree->rstartree->info->height] = false;
    }

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (fastrstartree->rstartree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(fastrstartree->rstartree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

/* FAST Hilbert R-tree */
void hh_write_fasthilbertrtree_header(const char *path, const FASTHilbertRTree *fastr) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;
    HilbertRTree *r;
    uint8_t type = FAST_HILBERT_RTREE_TYPE;

    r = fastr->hilbertrtree;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_hilbertrtreespec();
    bufsize += hh_get_size_fastspec(fastr->spec);

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rtreespec, fastspec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_hilbertrtreespec(r->spec, loc);
    loc += hh_serialize_fastspec(fastr->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_fastrtree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_fastrtree!", written, bufsize);
        return;
    }
    close(file);

    lwfree(buf);
}

SpatialIndex *hh_construct_fasthilbertrtree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    FASTIndex *fi;
    FASTHilbertRTree *fasthilbertrtree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_rtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = fasthilbertrtree_empty_create(index_path, src, gp, bs, NULL, false);

    //the we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    fi = (void *) si;
    fasthilbertrtree = fi->fast_index.fast_hilbertrtree;
    fasthilbertrtree->hilbertrtree->base.sc_id = si->sc_id;
    fasthilbertrtree->spec = (FASTSpecification*) lwalloc(sizeof (FASTSpecification));

    hh_set_rtrees_info_from_serialization(fasthilbertrtree->hilbertrtree->info, loc, &t_read);
    loc += t_read;

    hh_set_hilbertrtreespec_from_serialization(fasthilbertrtree->hilbertrtree->spec, loc, &t_read);
    loc += t_read;

    hh_set_fastspec_from_serialization(fasthilbertrtree->spec, loc, &t_read);
    loc += t_read;

    //now we have to read the root node
    fasthilbertrtree->hilbertrtree->current_node = (HilbertRNode *) fb_retrieve_node(&fi->base,
            fasthilbertrtree->hilbertrtree->info->root_page, fasthilbertrtree->hilbertrtree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (fasthilbertrtree->hilbertrtree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(fasthilbertrtree->hilbertrtree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

/* FOR-TREE */
void hh_write_fortree_header(const char *path, const FORTree *r) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_fortreespec();

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(r->type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rtreespec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_fortreespec(r->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_fortree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_fortree!", written, bufsize);
        return;
    }
    close(file);

    lwfree(buf);
}

SpatialIndex *hh_construct_fortree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    FORTree *fortree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_rtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = fortree_empty_create(index_path, src, gp, bs, NULL, false);

    //the we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    fortree = (void *) si;

    hh_set_rtrees_info_from_serialization(fortree->info, loc, &t_read);
    loc += t_read;

    fortree->spec = (FORTreeSpecification*) lwalloc(sizeof (FORTreeSpecification));

    hh_set_fortreespec_from_serialization(fortree->spec, loc, &t_read);
    loc += t_read;

    //now we have to read the root node
    fortree->current_node = forb_retrieve_rnode(&fortree->base, fortree->info->root_page, fortree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (fortree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(fortree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

/* eFIND R-TREE */
void hh_write_efindrtree_header(const char *path, const eFINDRTree *efindr) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;
    RTree *r;
    uint8_t type = eFIND_RTREE_TYPE;

    r = efindr->rtree;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_rtreespec();
    bufsize += hh_get_size_efindspec(efindr->spec);

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rtreespec, efindspec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_rtreespec(r->spec, loc);
    loc += hh_serialize_efindspec(efindr->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_efindrtree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_efindrtree!", written, bufsize);
        return;
    }
    close(file);
    /*
    _DEBUGF(NOTICE, "%d, %d, %d, %d", efindr->rtree->info->root_page, efindr->rtree->info->height,
           efindr->rtree->info->last_allocated_page, efindr->rtree->info->nof_empty_pages);

    _DEBUGF(NOTICE, "%d, %d, %d, %d, %d", efindr->rtree->spec->max_entries_int_node, efindr->rtree->spec->max_entries_leaf_node,
            efindr->rtree->spec->min_entries_int_node, efindr->rtree->spec->min_entries_leaf_node,
            efindr->rtree->spec->or_id);

    _DEBUGF(NOTICE, "%d, %d, %d, %s, %zu, %zu, %zu", efindr->spec->flushing_policy,
            efindr->spec->flushing_unit_size, efindr->spec->index_sc_id, efindr->spec->log_file,
            efindr->spec->log_size, efindr->spec->offset_last_elem_log, efindr->spec->size_last_elem_log);
     */

    lwfree(buf);
}

SpatialIndex *hh_construct_efindrtree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    eFINDIndex *fi;
    eFINDRTree *efindrtree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_rtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = efindrtree_empty_create(index_path, src, gp, bs, NULL, false);

    //the we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    fi = (void *) si;
    efindrtree = fi->efind_index.efind_rtree;
    efindrtree->rtree->base.sc_id = si->sc_id;
    efindrtree->spec = (eFINDSpecification*) lwalloc(sizeof (eFINDSpecification));

    hh_set_rtrees_info_from_serialization(efindrtree->rtree->info, loc, &t_read);
    loc += t_read;

    hh_set_rtreespec_from_serialization(efindrtree->rtree->spec, loc, &t_read);
    loc += t_read;

    hh_set_efindspec_from_serialization(efindrtree->spec, loc, &t_read);
    loc += t_read;

    //we should set the sizes of the 2Q properly
    if (efindrtree->spec->read_buffer_policy == eFIND_2Q_RBP) {
        efind_readbuffer_2q_setsizes(efindrtree->spec, gp->page_size);
    }
    /*
        _DEBUGF(NOTICE, "%d, %d, %d, %d", efindrtree->rtree->info->root_page, efindrtree->rtree->info->height,
               efindrtree->rtree->info->last_allocated_page, efindrtree->rtree->info->nof_empty_pages);

        _DEBUGF(NOTICE, "%d, %d, %d, %d, %d", efindrtree->rtree->spec->max_entries_int_node, efindrtree->rtree->spec->max_entries_leaf_node,
                efindrtree->rtree->spec->min_entries_int_node, efindrtree->rtree->spec->min_entries_leaf_node,
                efindrtree->rtree->spec->or_id);

        _DEBUGF(NOTICE, "%d, %d, %d, %s, %zu, %zu, %zu", efindrtree->spec->flushing_policy,
                efindrtree->spec->flushing_unit_size, efindrtree->spec->index_sc_id, efindrtree->spec->log_file,
                efindrtree->spec->log_size, efindrtree->spec->offset_last_elem_log, efindrtree->spec->size_last_elem_log);
    
        _DEBUGF(NOTICE, "%zu, %f, %d, %zu, %d, %d, %d, %d", efindrtree->spec->read_buffer_size, efindrtree->spec->read_tc_threshold,
                efindrtree->spec->read_temporal_control_size, efindrtree->spec->write_buffer_size, efindrtree->spec->write_tc_minimum_distance,
                efindrtree->spec->write_tc_stride, efindrtree->spec->write_temporal_control_size, efindrtree->spec->temporal_control_policy);
     */

    //now we have to read the root node
    efindrtree->rtree->current_node = (RNode *) efind_buf_retrieve_node(&fi->base, efindrtree->spec,
            efindrtree->rtree->info->root_page, efindrtree->rtree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (efindrtree->rtree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(efindrtree->rtree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

/* eFIND R*-TREE */
void hh_write_efindrstartree_header(const char *path, const eFINDRStarTree *efindrstar) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;
    RStarTree *r;
    uint8_t type = eFIND_RSTARTREE_TYPE;

    r = efindrstar->rstartree;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_rstartreespec();
    bufsize += hh_get_size_efindspec(efindrstar->spec);

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rstartreespec, efindspec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_rstartreespec(r->spec, loc);
    loc += hh_serialize_efindspec(efindrstar->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_efindrstartree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_efindrstartree!", written, bufsize);
        return;
    }
    close(file);

    lwfree(buf);
}

SpatialIndex *hh_construct_efindrstartree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    eFINDIndex *fi;
    eFINDRStarTree *efindrstartree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_rtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = efindrstartree_empty_create(index_path, src, gp, bs, NULL, false);

    //then we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    fi = (void *) si;
    efindrstartree = fi->efind_index.efind_rstartree;
    efindrstartree->rstartree->base.sc_id = si->sc_id;
    efindrstartree->spec = (eFINDSpecification*) lwalloc(sizeof (eFINDSpecification));

    hh_set_rtrees_info_from_serialization(efindrstartree->rstartree->info, loc, &t_read);
    loc += t_read;

    hh_set_rstartreespec_from_serialization(efindrstartree->rstartree->spec, loc, &t_read);
    loc += t_read;

    hh_set_efindspec_from_serialization(efindrstartree->spec, loc, &t_read);
    loc += t_read;

    //we should set the sizes of the 2Q properly
    if (efindrstartree->spec->read_buffer_policy == eFIND_2Q_RBP) {
        efind_readbuffer_2q_setsizes(efindrstartree->spec, gp->page_size);
    }

    //now we have to read the root node
    efindrstartree->rstartree->current_node = (RNode *) efind_buf_retrieve_node(&fi->base, efindrstartree->spec,
            efindrstartree->rstartree->info->root_page, efindrstartree->rstartree->info->height);

    //if we have a rstartree with height greater than 1
    if (efindrstartree->rstartree->info->height >= 1) {
        int i;
        efindrstartree->rstartree->reinsert = (bool*) lwrealloc(efindrstartree->rstartree->reinsert,
                sizeof (bool) * (efindrstartree->rstartree->info->height + 1));
        for (i = 0; i < efindrstartree->rstartree->info->height; i++)
            efindrstartree->rstartree->reinsert[i] = true;
        efindrstartree->rstartree->reinsert[efindrstartree->rstartree->info->height] = false;
    }

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (efindrstartree->rstartree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(efindrstartree->rstartree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

void hh_write_efindhilbertrtree_header(const char *path, const eFINDHilbertRTree *efindr) {
    uint8_t *buf, *loc;
    size_t bufsize;
    size_t finalsize;
    size_t written;
    int file;
    HilbertRTree *r;
    uint8_t type = eFIND_HILBERT_RTREE_TYPE;

    r = efindr->hilbertrtree;

    /* size of header, type of index and other sizes */
    bufsize = sizeof (uint8_t) + sizeof (size_t);
    bufsize += hh_get_size_source(r->base.src);
    bufsize += hh_get_size_generic_spec(r->base.gp);
    bufsize += hh_get_size_buffer_spec(r->base.bs);
    bufsize += hh_get_size_other_param();
    bufsize += hh_get_size_rtrees_info(r->info);
    bufsize += hh_get_size_hilbertrtreespec();
    bufsize += hh_get_size_efindspec(efindr->spec);

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //we firstly write the total size of this header
    memcpy(loc, &(bufsize), sizeof (size_t));
    loc += sizeof (size_t);

    //we then write the type of index
    memcpy(loc, &(type), sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //then we write the remaining data: source, genericparameters, bufferspec, rtreesinfo, rtreespec, efindspec
    loc += hh_serialize_source(r->base.src, loc);
    loc += hh_serialize_generic_spec(r->base.gp, loc);
    loc += hh_serialize_buffer_spec(r->base.bs, loc);
    loc += hh_serialize_other_param(&r->base, loc);
    loc += hh_serialize_rtrees_info(r->info, loc);
    loc += hh_serialize_hilbertrtreespec(r->spec, loc);
    loc += hh_serialize_efindspec(efindr->spec, loc);
    finalsize = (size_t) (loc - buf);

    if (bufsize != finalsize) /* Uh oh! */ {
        _DEBUGF(ERROR, "Return size (%zu) not equal to expected size (%zu) in write_efindrtree!", finalsize, bufsize);
        return;
    }

    file = hh_spc_open(path);
    if ((written = write(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in write_efindrtree!", written, bufsize);
        return;
    }
    close(file);

    lwfree(buf);
}

SpatialIndex *hh_construct_efindhilbertrtree_from_header(const char *path) {
    uint8_t *loc, *buf;
    size_t bufsize;
    size_t t_read;
    int file;
    int aux;
    char *index_path;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
    eFINDIndex *fi;
    eFINDHilbertRTree *efindhilbertrtree;
    SpatialIndex *si;

    /* getting the index name */
    aux = strlen(path) - strlen(".header");
    index_path = (char*) lwalloc(sizeof (char) * aux + 1);
    memcpy(index_path, path, aux);
    index_path[aux] = (char) 0;

    bufsize = hh_get_total_size(path);

    buf = (uint8_t*) lwalloc(bufsize);

    file = hh_spc_open(path);
    if ((t_read = read(file, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in read_rtreespec!", t_read, bufsize);
        return NULL;
    }
    close(file);

    loc = buf;

    //we firstly jump the size of the header
    loc += sizeof (size_t);
    //we jump the idx_type
    loc += sizeof (uint8_t);

    src = hh_get_source_from_serialization(loc, &t_read);
    loc += t_read;

    gp = hh_get_generic_spec_from_serialization(loc, &t_read);
    loc += t_read;

    //then we read the buffer specification
    bs = hh_get_buffer_spec_from_serialization(loc, &t_read);
    loc += t_read;

    si = efindhilbertrtree_empty_create(index_path, src, gp, bs, NULL, false);

    //the we read the sc_id
    si->sc_id = hh_get_sc_id(loc, &t_read);
    loc += t_read;

    fi = (void *) si;
    efindhilbertrtree = fi->efind_index.efind_hilbertrtree;
    efindhilbertrtree->hilbertrtree->base.sc_id = si->sc_id;
    efindhilbertrtree->spec = (eFINDSpecification*) lwalloc(sizeof (eFINDSpecification));

    hh_set_rtrees_info_from_serialization(efindhilbertrtree->hilbertrtree->info, loc, &t_read);
    loc += t_read;

    hh_set_hilbertrtreespec_from_serialization(efindhilbertrtree->hilbertrtree->spec, loc, &t_read);
    loc += t_read;

    hh_set_efindspec_from_serialization(efindhilbertrtree->spec, loc, &t_read);
    loc += t_read;

    //we should set the sizes of the 2Q properly
    if (efindhilbertrtree->spec->read_buffer_policy == eFIND_2Q_RBP) {
        efind_readbuffer_2q_setsizes(efindhilbertrtree->spec, gp->page_size);
    }

    //now we have to read the root node
    efindhilbertrtree->hilbertrtree->current_node = (HilbertRNode *) efind_buf_retrieve_node(&fi->base, efindhilbertrtree->spec,
            efindhilbertrtree->hilbertrtree->info->root_page, efindhilbertrtree->hilbertrtree->info->height);

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        if (efindhilbertrtree->hilbertrtree->info->height > 0) {
            //we visited one internal node, then we add it
            _visited_int_node_num++;
        } else {
            //we visited one leaf node
            _visited_leaf_node_num++;
        }
        insert_reads_per_height(efindhilbertrtree->hilbertrtree->info->height, 1);
    }
#endif

    lwfree(buf);
    return si;
}

/*this function is only called when performing flushing ALL modifications
 after calling it, the SpatialIndex object can be destroyed */
void festival_header_writer(const char *idx_spc_path, uint8_t type, SpatialIndex *si) {
    HeaderBuffer *hash_entry;
    switch (type) {
        case CONVENTIONAL_RTREE:
        {
            RTree *rtree = (void *) si;
            hh_write_rtree_header(idx_spc_path, rtree);
            break;
        }
        case CONVENTIONAL_RSTARTREE:
        {
            RStarTree *rstartree = (void *) si;
            hh_write_rstartree_header(idx_spc_path, rstartree);
            break;
        }
        case CONVENTIONAL_HILBERT_RTREE:
        {
            HilbertRTree *hilbertrtree = (void*) si;
            hh_write_hilbertrtree_header(idx_spc_path, hilbertrtree);
            break;
        }
        case FAST_RTREE_TYPE:
        {
            FASTIndex *fi = (void *) si;
            hh_write_fastrtree_header(idx_spc_path, fi->fast_index.fast_rtree);
            break;
        }
        case FAST_RSTARTREE_TYPE:
        {
            FASTIndex *fi = (void *) si;
            hh_write_fastrstartree_header(idx_spc_path, fi->fast_index.fast_rstartree);
            break;
        }
        case FAST_HILBERT_RTREE_TYPE:
        {
            FASTIndex *fi = (void *) si;
            hh_write_fasthilbertrtree_header(idx_spc_path, fi->fast_index.fast_hilbertrtree);
            break;
        }
        case FORTREE_TYPE:
        {
            FORTree *fortree = (void *) si;
            hh_write_fortree_header(idx_spc_path, fortree);
            break;
        }
        case eFIND_RTREE_TYPE:
        {
            eFINDIndex *fi = (void *) si;
            hh_write_efindrtree_header(idx_spc_path, fi->efind_index.efind_rtree);
            break;
        }
        case eFIND_RSTARTREE_TYPE:
        {
            eFINDIndex *fi = (void *) si;
            hh_write_efindrstartree_header(idx_spc_path, fi->efind_index.efind_rstartree);
            break;
        }
        case eFIND_HILBERT_RTREE_TYPE:
        {
            eFINDIndex *fi = (void *) si;
            hh_write_efindhilbertrtree_header(idx_spc_path, fi->efind_index.efind_hilbertrtree);
            break;
        }
        default:
        {
            _DEBUGF(ERROR, "There is no this index (%d) type yet:", type);
        }
    }

    //then we check if we need to remove it from our HeaderBuffer
    HASH_FIND_STR(headers, idx_spc_path, hash_entry);
    if (hash_entry != NULL) {
        HASH_DEL(headers, hash_entry);
        lwfree(hash_entry->path);
        lwfree(hash_entry);
        //the SpatialIndex is freed outside of this function!
    }
}

static SpatialIndex *get_from_headerbuffer(const char *path) {
    HeaderBuffer *hash_entry;
    SpatialIndex *si = NULL;

    HASH_FIND_STR(headers, path, hash_entry);
    if (hash_entry != NULL) {
        int h;
        uint8_t idx_type = spatialindex_get_type(hash_entry->si);

        si = hash_entry->si;
        
        //_DEBUGF(NOTICE, "Recovering the header for %s", hash_entry->path);

        //now we have to put the root node
        switch (idx_type) {
            case CONVENTIONAL_RTREE:
            {
                RTree *rtree = (void *) si;
                h = rtree->info->height;
                if (rtree->current_node != NULL) {
                    rnode_free(rtree->current_node);
                }
                rtree->current_node = get_rnode(si, rtree->info->root_page, rtree->info->height);
                break;
            }
            case CONVENTIONAL_RSTARTREE:
            {
                RStarTree *rstartree = (void *) si;
                h = rstartree->info->height;
                if (rstartree->current_node != NULL) {
                    rnode_free(rstartree->current_node);
                }
                rstartree->current_node = get_rnode(si, rstartree->info->root_page, rstartree->info->height);

                if (rstartree->info->height >= 1) {
                    int i;
                    for (i = 0; i < rstartree->info->height; i++)
                        rstartree->reinsert[i] = true;
                    rstartree->reinsert[rstartree->info->height] = false;
                }
                break;
            }
            case CONVENTIONAL_HILBERT_RTREE:
            {
                HilbertRTree *hrtree = (void *) si;
                h = hrtree->info->height;
                if (hrtree->current_node != NULL) {
                    hilbertnode_free(hrtree->current_node);
                }
                hrtree->current_node = get_hilbertnode(si, hrtree->info->root_page, hrtree->info->height);
                break;
            }
            case FAST_RTREE_TYPE:
            {
                FASTIndex *fast = (void *) si;
                FASTRTree *fastrtree = fast->fast_index.fast_rtree;
                h = fastrtree->rtree->info->height;
                if (fastrtree->rtree->current_node != NULL) {
                    rnode_free(fastrtree->rtree->current_node);
                }
                fastrtree->rtree->current_node = (RNode *) fb_retrieve_node(&fast->base,
                        fastrtree->rtree->info->root_page, fastrtree->rtree->info->height);
                break;
            }
            case FAST_RSTARTREE_TYPE:
            {
                FASTIndex *fast = (void *) si;
                FASTRStarTree *fastrstartree = fast->fast_index.fast_rstartree;
                h = fastrstartree->rstartree->info->height;
                if (fastrstartree->rstartree->current_node != NULL) {
                    rnode_free(fastrstartree->rstartree->current_node);
                }
                fastrstartree->rstartree->current_node = (RNode *) fb_retrieve_node(&fast->base,
                        fastrstartree->rstartree->info->root_page, fastrstartree->rstartree->info->height);

                if (fastrstartree->rstartree->info->height >= 1) {
                    int i;
                    for (i = 0; i < fastrstartree->rstartree->info->height; i++)
                        fastrstartree->rstartree->reinsert[i] = true;
                    fastrstartree->rstartree->reinsert[fastrstartree->rstartree->info->height] = false;
                }
                break;
            }
            case FAST_HILBERT_RTREE_TYPE:
            {
                FASTIndex *fast = (void *) si;
                FASTHilbertRTree *fasthrtree = fast->fast_index.fast_hilbertrtree;
                h = fasthrtree->hilbertrtree->info->height;
                if (fasthrtree->hilbertrtree->current_node != NULL) {
                    hilbertnode_free(fasthrtree->hilbertrtree->current_node);
                }
                fasthrtree->hilbertrtree->current_node = (HilbertRNode *) fb_retrieve_node(&fast->base,
                        fasthrtree->hilbertrtree->info->root_page,
                        fasthrtree->hilbertrtree->info->height);
                break;
            }
            case FORTREE_TYPE:
            {
                FORTree *fortree = (void *) si;
                h = fortree->info->height;
                if (fortree->current_node != NULL) {
                    rnode_free(fortree->current_node);
                }
                fortree->current_node = forb_retrieve_rnode(&fortree->base,
                        fortree->info->root_page, fortree->info->height);
                break;
            }
            case eFIND_RTREE_TYPE:
            {
                eFINDIndex *ei = (void *) si;
                eFINDRTree *efindrtree = ei->efind_index.efind_rtree;
                h = efindrtree->rtree->info->height;
                if (efindrtree->rtree->current_node != NULL) {
                    rnode_free(efindrtree->rtree->current_node);
                }
                efindrtree->rtree->current_node = (RNode *) efind_buf_retrieve_node(&ei->base, efindrtree->spec,
                        efindrtree->rtree->info->root_page, efindrtree->rtree->info->height);
                break;
            }
            case eFIND_RSTARTREE_TYPE:
            {
                eFINDIndex *ei = (void *) si;
                eFINDRStarTree *efindrstartree = ei->efind_index.efind_rstartree;
                h = efindrstartree->rstartree->info->height;
                if (efindrstartree->rstartree->current_node != NULL) {
                    rnode_free(efindrstartree->rstartree->current_node);
                }
                efindrstartree->rstartree->current_node = (RNode *) efind_buf_retrieve_node(&ei->base, efindrstartree->spec,
                        efindrstartree->rstartree->info->root_page, efindrstartree->rstartree->info->height);

                if (efindrstartree->rstartree->info->height >= 1) {
                    int i;
                    for (i = 0; i < efindrstartree->rstartree->info->height; i++)
                        efindrstartree->rstartree->reinsert[i] = true;
                    efindrstartree->rstartree->reinsert[efindrstartree->rstartree->info->height] = false;
                }
                break;
            }
            case eFIND_HILBERT_RTREE_TYPE:
            {
                eFINDIndex *ei = (void *) si;
                eFINDHilbertRTree *efindhilbertrtree = ei->efind_index.efind_hilbertrtree;
                if (efindhilbertrtree->hilbertrtree->current_node != NULL) {
                    hilbertnode_free(efindhilbertrtree->hilbertrtree->current_node);
                }
                h = efindhilbertrtree->hilbertrtree->info->height;
                efindhilbertrtree->hilbertrtree->current_node = (HilbertRNode *) efind_buf_retrieve_node(&ei->base, efindhilbertrtree->spec,
                        efindhilbertrtree->hilbertrtree->info->root_page, efindhilbertrtree->hilbertrtree->info->height);
                break;
            }
            default:
            {
                _DEBUGF(ERROR, "There is no this index (%d) type yet:", idx_type);
            }
        }

#ifdef COLLECT_STATISTICAL_DATA
        if (_STORING == 0) {
            if (h > 0) {
                //we visited one internal node, then we add it
                _visited_int_node_num++;
            } else {
                //we visited one leaf node
                _visited_leaf_node_num++;
            }
            insert_reads_per_height(h, 1);
        }
#endif
    }

    return si;
}

SpatialIndex *festival_get_spatialindex(const char *idx_spc_path) {
    uint8_t idx_type;
    SpatialIndex *si;
    char *key;
    HeaderBuffer *hash_entry;

    //check if this header is stored in our in-memory buffer
    si = get_from_headerbuffer(idx_spc_path);
    if (si != NULL) {
        return si;
    }
    //otherwise, we should recover it from the header file

    idx_type = hh_get_index_type(idx_spc_path);

    /*a jump table is not the best approach here because 
     * most probably the compiler will compile it using jumps and inline calls
     * instead of call functions made by hand (in customs jump tables)*/
    switch (idx_type) {
        case CONVENTIONAL_RTREE:
        {
            si = hh_construct_rtree_from_header(idx_spc_path);
            break;
        }
        case CONVENTIONAL_RSTARTREE:
        {
            si = hh_construct_rstartree_from_header(idx_spc_path);
            break;
        }
        case CONVENTIONAL_HILBERT_RTREE:
        {
            si = hh_construct_hilbertrtree_from_header(idx_spc_path);
            break;
        }
        case FAST_RTREE_TYPE:
        {
            si = hh_construct_fastrtree_from_header(idx_spc_path);
            break;
        }
        case FAST_RSTARTREE_TYPE:
        {
            si = hh_construct_fastrstartree_from_header(idx_spc_path);
            break;
        }
        case FAST_HILBERT_RTREE_TYPE:
        {
            si = hh_construct_fasthilbertrtree_from_header(idx_spc_path);
            break;
        }
        case FORTREE_TYPE:
        {
            si = hh_construct_fortree_from_header(idx_spc_path);
            break;
        }
        case eFIND_RTREE_TYPE:
        {
            si = hh_construct_efindrtree_from_header(idx_spc_path);
            break;
        }
        case eFIND_RSTARTREE_TYPE:
        {
            si = hh_construct_efindrstartree_from_header(idx_spc_path);
            break;
        }
        case eFIND_HILBERT_RTREE_TYPE:
        {
            si = hh_construct_efindhilbertrtree_from_header(idx_spc_path);
            break;
        }
        default:
        {
            _DEBUGF(ERROR, "There is no this index (%d) type yet:", idx_type);
        }
    }

    //now we keep it in our in-memory buffer
    hash_entry = (HeaderBuffer*) lwalloc(sizeof (HeaderBuffer));
    key = lwalloc(strlen(idx_spc_path) + 1);
    strcpy(key, idx_spc_path);
    hash_entry->path = key;
    hash_entry->si = si;

    HASH_ADD_KEYPTR(hh, headers, hash_entry->path, strlen(hash_entry->path), hash_entry);

    return si;
}

bool spatialindex_keep_header(const char *idx_spc_path, SpatialIndex *si) {
    HeaderBuffer *hash_entry;
    char *key;

    HASH_FIND_STR(headers, idx_spc_path, hash_entry);
    if (hash_entry != NULL) {
        //this means that this index already has a header here
        return false;
    }

    hash_entry = (HeaderBuffer*) lwalloc(sizeof (HeaderBuffer));
    key = lwalloc(strlen(idx_spc_path) + 1);
    strcpy(key, idx_spc_path);
    hash_entry->path = key;
    hash_entry->si = si;

    HASH_ADD_KEYPTR(hh, headers, hash_entry->path, strlen(hash_entry->path), hash_entry);
    return true;
}
