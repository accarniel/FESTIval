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
 * File:   spatial_index.h
 * Author: Anderson Chaves Carniel
 *
 * Created on September 24, 2016, 6:07 PM
 */

#ifndef SPATIAL_INDEX_SPEC_H
#define SPATIAL_INDEX_SPEC_H

#include <liblwgeom.h> /* for LWGEOM */
#include "festival_defs.h"

/* the refinement step is performed by using geometrical computation from GEOS
 * it accesses the geometries in the postgres (by using their primary keys)
 * it returns a set of geometries
 *  */

/*Types of refinements */
#define ONLY_GEOS               1 //this means that only the geos is used without any improvement
#define GEOS_AND_POINT_POLYGON  2 //this means that geos and the postgis point_in_polygon is used

/* an index is constructed and accesses information from a dataset 
 the following struct corresponds to the table Source of FESTIval data schema
 */
typedef struct {
    char *schema; //the schema where the spatial objects are stored
    char *table; //table
    char *column; //column in which the geometries were indexed
    char *pk; //primary key of the indexed table    
    int src_id; //the primary key of the table Source
} Source;

/*definition of the type of the storage system */
#define HDD			1
#define SSD 			2
#define FLASHDBSIM		3 

typedef struct {
    uint8_t type; //can be HDD, SSD, or a flash simulator (currently FlashDBSim)   (see above) 
    int ss_id; //the identifier of the storage system (stored in the FESTIval data schema)
    void *info; //this is a void pointer to store other possible information (see below)
} StorageSystem;

/*should we store the information with respect to the HDD or not?*/
typedef struct {
    /*info with respect to the VFD*/
    int nand_device_type;
    int block_count;
    int page_count_per_block;
    int page_size1;
    int page_size2;
    int erase_limitation;
    int read_random_time;
    int read_serial_time;
    int program_time;
    int erase_time;
    /*info with respect to the FTL*/
    int ftl_type;
    int map_list_size;
    int wear_leveling_threshold;
} FlashDBSim;

/* all indices have generic parameters, which are represented by the following structure
 * it corresponds to the table BasicConfiguration of FESTIval data schema */
typedef struct {
    StorageSystem *storage_system; //where the index is stored
    uint8_t io_access; //the type of access for the disk (see io_handler.h)
    int page_size; //how many bytes we will consider to store the nodes?
    uint8_t refinement_type; //the refinement type of this configuration (see above)
    int bc_id; //the primary key of the table BasicConfiguration
} GenericParameters;

/* an index can have a buffer in the main memory to manage its data.
 Some important notes for flash-aware spatial indices that use its own buffers:
 * (i) this buffer is different from the flash-aware spatial indices buffer
 * (ii) this buffer is used after the buffer of the flash-aware spatial index,
 * this means that when the flash-aware spatial index sends a write, 
 * the data will be firstly stored in the own buffer of the flash-aware spatial index 
 * (iii) when a flushing operation is done, the data will be stored in this buffer. 
 * The positive side is that more space in the main memory is used to manage the index.
 * The negative side is that a flushing operation that writes several nodes 
 * may not indeed perform sequential writes in the disk. 
 */
#define BUFFER_NONE         0 //there is no buffer for the index
#define BUFFER_LRU          1 //the traditional LRU cache
#define BUFFER_HLRU         2 //the traditional LRU cache that considers the height of the nodes
#define BUFFER_S2Q          3 //the simplified 2Q cache, see s2q.c
#define BUFFER_2Q           4 //the full version of 2Q cache, see full2q.c

typedef struct {
    uint8_t buffer_type; //type of this buffer (see above)
    size_t min_capacity; //the minimum capacity in bytes of this buffer
    size_t max_capacity; //the maximum capacity in bytes of this buffer
    int buf_id; //the primary key of the table BufferConfiguration
    void *buf_additional_param; //additional possible parameters    
} BufferSpecification;

typedef struct {
    size_t A1_size;
    size_t Am_size;
} BufferS2QSpecification;

typedef struct {
    size_t A1in_size;
    size_t A1out_size;
    size_t Am_size;
} Buffer2QSpecification;

/************************************
 STRUCT FOR QUERY PROCESSING
 ************************************/

/* this struct is responsible to manage the query results from the index structure */
typedef struct {
    int *row_id; //array of row identifiers
    int num_entries; //the number of entries
    int max; //maximum of entries
    bool final_result; //do these entries correspond to the final result of the query?
} SpatialIndexResult;

/************************************
 GENERIC SPATIAL INDEX STRUCT FOR FESTIval
 ************************************/

/*this struct defines a generic spatial_index data type
 this means that SpatialIndex is a generic index
 * it allows to multiple dynamic dispatching (polymorphism)
 */
typedef struct {
    const struct _SpatialIndexInterface * const vtable;    
    int sc_id; //the primary key of the table SpecializedConfiguration (it is stored here because here is where the type of the index is defined)
    char *index_file;
    Source *src;
    GenericParameters *gp;
    BufferSpecification *bs;
} SpatialIndex;

typedef struct _SpatialIndexInterface {
    /*get a unique identifier of the spatial index*/
    uint8_t (*get_type)(const SpatialIndex *si);
    /*insert/remove new/old entry in an index: 
     *  first parameter is the self index, 
     *  the second is a pointer (identifier) of the object,
     *  the third parameter is a POSTGIS object 
     * */
    bool (*insert)(SpatialIndex *si, int pointer, const LWGEOM *geom);
    bool (*remove)(SpatialIndex *si, int pointer, const LWGEOM *geom);
    /*update an object (old) to another (new)*/
    bool (*update)(SpatialIndex *si, int oldpointer, const LWGEOM *oldgeom,
            int newpointer, const LWGEOM *newgeom);
    /*search a index (SPATIAL SELECTION! CONSIDERING SPATIAL OBJECTS AS INPUT!):
     * for spatial joins, knn queries, use other functions! 
     *  first parameter is the self index, 
     *  the second is the LWGEOM object for the query 
     * (if it is a point, then the minimum and maximum coordinates of each axis are equal)
     *  the third parameter is the predicate to be considered (see bbox_handler.h)
     * */
    SpatialIndexResult* (*search_ss)(SpatialIndex *si, const LWGEOM *search_object, uint8_t predicate);
    /* write the header of the index in a specified file that contains specific info about the index
     * the first parameter is the self index while the second is the header file
     * */
    bool (*write_header)(SpatialIndex *si, const char *file);
    /* destroy (that is, free the index) */
    void (*destroy)(SpatialIndex *si);
} SpatialIndexInterface;



/************************************
             WRAPPERS
************************************/
static inline uint8_t spatialindex_get_type(const SpatialIndex *s) {
    return s->vtable->get_type(s);
}

static inline bool spatialindex_insert(SpatialIndex *s, int p, const LWGEOM *g) {
    return s->vtable->insert(s, p, g);
}

static inline bool spatialindex_remove(SpatialIndex *s, int p, const LWGEOM *g) {
    return s->vtable->remove(s, p, g);
}

static inline bool spatialindex_update(SpatialIndex *s, int old_p, const LWGEOM *old_g,
        int new_p, const LWGEOM *new_g) {
    return s->vtable->update(s, old_p, old_g, new_p, new_g);
}

static inline SpatialIndexResult *spatialindex_spatial_selection(SpatialIndex *s, const LWGEOM *so, uint8_t p) {
    return s->vtable->search_ss(s, so, p);
}

static inline bool spatialindex_header_writer(SpatialIndex *s, const char *file) {
    return s->vtable->write_header(s, file);
}

static inline void spatialindex_destroy(SpatialIndex *s) {
    s->vtable->destroy(s);
}


/* it reads the header of the index and returns as a spatial_index object
 * the first parameter is the header file
 * The returning SpatialIndex should be only freed when spatialindex_header_writer is called!
 * spatialindex_header_writer is responsible to free it from the in-memory buffer
 * and store the header as a file
 * */
typedef SpatialIndex* (*construct_from_header)(const char *file);

extern SpatialIndex *spatialindex_from_header(const char *file);

/* this function puts the SpatialIndex object being used in the buffer*/
extern bool spatialindex_keep_header(const char *idx_spc_path, SpatialIndex *si);

/*we can change the constructor that reads the header file */
extern void index_specification_set_constructor(construct_from_header constructor);


/***********************************************************************
 FUNCTIONS TO MANAGE OTHER VARIABLES (SOURCE AND GENERIC_PARAMETERS)
 ***********************************************************************/

/* manage memory of source and basicparameters */

/*(it does not copy the content of the strings - it only copies the reference) */
extern Source *create_source(char *schema, char *table, char *column, char *pk);
extern void source_free(Source *src);

extern GenericParameters *generic_parameters_create(StorageSystem *ss, uint8_t io,
        int ps, uint8_t ref);
extern void generic_parameters_free(GenericParameters *gp);

/*this function adds a row_id to the result of a query */
extern SpatialIndexResult *spatial_index_result_create(void);
extern void spatial_index_result_add(SpatialIndexResult *sir, int row_id);
extern void spatial_index_result_free(SpatialIndexResult *sir);

#endif /* SPATIAL_INDEX_SPEC_H */
