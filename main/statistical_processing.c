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
#include "executor/spi.h"
#include <stringbuffer.h>
#include <libgen.h>
#include <stdio.h>
#include <stdint.h>

#include "statistical_processing.h"
#include "bbox_handler.h"
#include "spatial_approximation.h"

#include "log_messages.h"
#include "storage_handler.h"
#include "../rtree/rtree.h"
#include "../rtree/rnode.h"
#include "../rstartree/rstartree.h" 
#include "../hilbertrtree/hilbertrtree.h"
#include "../fast/fast_index.h"
#include "../fast/fast_buffer.h"
#include "../fortree/fortree.h"
#include "../fortree/fortree_buffer.h"
#include "../efind/efind.h"
#include "../efind/efind_buffer_manager.h"

/*to get statistical info from the flashdbsim simulator*/
#include "FlashDBSim_capi.h"

/*a global variable to indicate that we are storing statistical values*/
uint8_t _STORING = 0;
/*a global variable to indicate that we have to store the order of read/write operations and store it later*/
uint8_t _COLLECT_READ_WRITE_ORDER = 0;

//only the function STI_set_execution_name handles this value!
char *_execution_name = NULL; //the execution name of the workload (done)

uint8_t _query_predicate = INTERSECTS; //the predicate when the operation is a query (e.g., OVERLAPS)

/* variables to manage time in ms (elapsed time) */
double _total_time = 0.0; //total time to completely process an operation (e.g., a range query)
double _index_time = 0.0; //time that the spatial index took to process an operation (done)
double _filter_time = 0.0; //time for the filter step for a spatial query with index (done)
double _refinement_time = 0.0; //time for the refinement step for a spatial query with index (this indicates the time of 9-IM processing) (done)
double _retrieving_objects_time = 0.0;
double _processing_predicates_time = 0.0;
double _read_time = 0.0; //total time for all performed read operations (done)
double _write_time = 0.0; //total time for all performed write operations (done)
double _split_time = 0.0; //total time for all performed split operations (done)

/* variables to manage time in ms (CPU time) */
double _total_cpu_time = 0.0; //total time to completely process an operation (e.g., a range query)
double _index_cpu_time = 0.0; //time that the spatial index took to return the set of candidates (done)
double _filter_cpu_time = 0.0; //time for the filter step for a spatial query with index (done)
double _refinement_cpu_time = 0.0; //time for the refinement step for a spatial query with index (this indicates the time of 9-IM processing) (done)
double _retrieving_objects_cpu_time = 0.0;
double _processing_predicates_cpu_time = 0.0;
double _read_cpu_time = 0.0; //total time for all performed read operations (done)
double _write_cpu_time = 0.0; //total time for all performed write operations (done)
double _split_cpu_time = 0.0; //total time for all performed split operations (done)

/* variables to manage numbers/amounts */
int _cand_num = 0; //number of candidates returned by the filtering step (done)
int _result_num = 0; //number of returned spatial objects of a query (done)
int _read_num = 0; //number of read operations (done)
int _write_num = 0; //number of write operations (done)
int _split_int_num = 0; //number of split operations done in the internal nodes (done)
int _split_leaf_num = 0; //number of split operations done in the leaf nodes (done)
unsigned long long int _processed_entries_num = 0;
int _reinsertion_num = 0; //number of times that the reinsertion policy was performed
int _visited_int_node_num = 0; //number of visited internal nodes in an operation --this is to ACCESS a node (done)
int _visited_leaf_node_num = 0; //number of visited leaf nodes in an operation --this is to ACCESS a node (done)
int _written_int_node_num = 0; //number of written internal nodes in an operation --this is to WRITE a node (done)
int _written_leaf_node_num = 0; //number of written leaf nodes in an operation --this is to WRITE a node (done)
int _deleted_int_node_num = 0; //number of deleted internal nodes in an operation --this is to REMOVE a node (done)
int _deleted_leaf_node_num = 0; //number of deleted leaf nodes in an operation --this is to REMOVE a node (done)

int _entries_int_nodes = 0; //number of occupied entries in internal nodes
int _entries_leaf_nodes = 0; //number of occupied entries in leaf nodes
int _internal_nodes_num = 0; //number of the created internal nodes
int _leafs_nodes_num = 0; //number of the created leaf nodes

double _flushing_time = 0.0; //time to flush a part of buffer to ssd
double _flushing_cpu_time = 0.0; //the same of previous but for cpu time
int _flushing_num = 0; //number of performed flushing
int _flushed_nodes_num = 0;

int _nof_unnecessary_flushed_nodes = 0; //number of unneeded writes performed by an index (done)

//total values
int _mod_node_buffer_num = 0; //TOTAL number of modified entries that passed in the buffer
int _new_node_buffer_num = 0; //TOTAL number of newly created nodes that was stored in the buffer
int _del_node_buffer_num = 0; //TOTAL number of removed nodes that was stored in the buffer
//current values (actual values)
int _cur_mod_node_buffer_num = 0; //number of modified entries in the buffer
int _cur_new_node_buffer_num = 0; //number of newly created nodes in the buffer
int _cur_del_node_buffer_num = 0; //number of removed nodes in the buffer
int _cur_buffer_size = 0; //the size in bytes of the buffer after an operation
double _write_log_time = 0.0; //total time to manage the writes performed on the log 
double _write_log_cpu_time = 0.0; //total CPU time to manage the writes performed on the log
double _ret_node_from_buf_time = 0.0; //total time that was waste to retrieve nodes from the buffer
double _ret_node_from_buf_cpu_time = 0.0; //total time that was waste to retrieve nodes from the buffer (cpu time))
double _compactation_log_time = 0.0; //time to compact the log (for durability)
double _compactation_log_cpu_time = 0.0; //the same of previous but for cpu time
double _recovery_log_time = 0.0; //time to recovery the buffer in main memory from log (for durability)
double _recovery_log_cpu_time = 0.0; //the same of previous but for cpu time
int _compactation_log_num = 0; //number of times that the compaction of log was done
int _write_log_num = 0; //number of writes in the log (to guarantee durability)
int _read_log_num = 0;
int _cur_log_size = 0; //the size in bytes of the log file after an operation

int _int_o_nodes_num = 0;
int _merge_back_num = 0;
int _entries_int_o_nodes = 0; //number of entries in internal o-nodes
int _entries_leaf_o_nodes = 0; //number of entries in leaf o-nodes
int _leaf_o_nodes_num = 0;

DynamicArrayInt *_writes_per_height = NULL;
DynamicArrayInt *_reads_per_height = NULL;
RWOrder *_rw_order = NULL;
int _height = 0; //height of the tree (done)

/* variable for the collection of statistical data of standard buffers (e.g., LRU) */
int _sbuffer_page_fault = 0;
int _sbuffer_page_hit = 0;
double _sbuffer_find_time = 0.0;
double _sbuffer_find_cpu_time = 0.0; //the time to read a node from disk and put it into the buffer
//this flushing time is different from the flushing time of flash-aware spatial indices
//since this is for the standard buffers
double _sbuffer_flushing_time = 0.0;
double _sbuffer_flushing_cpu_time = 0.0;

/* statistical values for efind indices*/
int _read_buffer_page_hit = 0; //number of hits done by the read buffer of the efind
int _read_buffer_page_fault = 0; //number of faults done by the read buffer of the efind
int _cur_read_buffer_size = 0; //read buffer size of the efind
double _read_buffer_put_node_cpu_time = 0.0; //consuming time to put a node in the read buffer
double _read_buffer_put_node_time = 0.0; //consuming time to put a node in the read buffer
double _read_buffer_get_node_cpu_time = 0.0; //consuming time to get a node from the read buffer
double _read_buffer_get_node_time = 0.0; //consuming time to get a node from the read buffer
int _efind_force_node_in_read_buffer = 0; //the number of times that the temporal control for reads was performed
int _efind_write_temporal_control_sequential = 0; //the number of that the temporal control for writes was performed (sequential version)
int _efind_write_temporal_control_stride = 0; //the number of that the temporal control for writes was performed (stride version)
int _efind_write_temporal_control_seqstride = 0; //the number of that the temporal control for writes was performed (mixed version)
int _efind_write_temporal_control_filled = 0; //the number of that the temporal control for writes had to be completed with random nodes

/*the other statistical values with respect to the flash simulator are extracted from the flashdbsim global variables*/

//local global variables
static DynamicArrayInt *_nodes_per_level = NULL; //number of allocated nodes per level of the tree (this is an array) (done)
static ArrayNode *_entries_per_node = NULL; //number of entries per node (this is an array) (done)
static ArrayNode *_area_per_node = NULL; //area for each node (this is an array) (done)
static ArrayNode *_ovp_area_per_node = NULL; //overlapping area per node (coverage) (this is an array) (done)
static ArrayNode *_dead_space_per_node = NULL; //dead space per node (this is an array) (done)

static eFINDSpecification *efind_spec = NULL;


/*this function returns the CPU time in order to compute the CPU time used*/
struct timespec get_CPU_time() {
    struct timespec time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);
    return time;
}

//this function returns the elapsed time in order to compute the total elapsed time of an operation
struct timespec get_current_time() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time;
}

//this function returns the elapsed time in order to compute the total elapsed time of an operation
double get_current_time_in_seconds() {
    struct timespec time;
    //we get the realtime here because this is for statistical data that uses real timestamps
    clock_gettime(CLOCK_REALTIME, &time);    
    return time.tv_sec + time.tv_nsec * 1e-9;
}

//calculate the total processed time and return the elapsed total time in SECONDS!
double get_elapsed_time(struct timespec start, struct timespec end) {
    double start_in_sec = (double)start.tv_sec + (double)start.tv_nsec / 1000000000.0;
    double end_in_sec = (double)end.tv_sec + (double)end.tv_nsec / 1000000000.0;
    return end_in_sec - start_in_sec;
}

void initiate_statistic_values() {
    int i;
    _writes_per_height = (DynamicArrayInt*) lwalloc(sizeof (DynamicArrayInt));
    _writes_per_height->maxelements = 30;
    _writes_per_height->nofelements = 0;
    _writes_per_height->array = (int*) lwalloc(sizeof (int) * _writes_per_height->maxelements);
    for (i = 0; i < 30; i++) {
        _writes_per_height->array[i] = 0;
    }

    _reads_per_height = (DynamicArrayInt*) lwalloc(sizeof (DynamicArrayInt));
    _reads_per_height->maxelements = 30;
    _reads_per_height->nofelements = 0;
    _reads_per_height->array = (int*) lwalloc(sizeof (int) * _reads_per_height->maxelements);
    for (i = 0; i < 30; i++) {
        _reads_per_height->array[i] = 0;
    }

    if (_COLLECT_READ_WRITE_ORDER == 1) {
        _rw_order = (RWOrder*) lwalloc(sizeof (RWOrder));
        _rw_order->maxelements = 30;
        _rw_order->nofelements = 0;
        _rw_order->node = (int*) lwalloc(sizeof (int)*_rw_order->maxelements);
        _rw_order->request_type = (uint8_t*) lwalloc(sizeof (uint8_t) * _rw_order->maxelements);
        _rw_order->time = (double*) lwalloc(sizeof (double)*_rw_order->maxelements);
    }
}

void insert_writes_per_height(int level, int incremented_v) {
    if (level >= _writes_per_height->maxelements) {
        _writes_per_height->maxelements *= 2;
        _writes_per_height->array = (int*) lwrealloc(_writes_per_height->array,
                sizeof (int)*_writes_per_height->maxelements);
    }
    _writes_per_height->array[level] += incremented_v;


    if ((level + 1) > _writes_per_height->nofelements) {
        _writes_per_height->nofelements = level + 1;
    }
}

void insert_reads_per_height(int level, int incremented_v) {
    if (level >= _reads_per_height->maxelements) {
        _reads_per_height->maxelements *= 2;
        _reads_per_height->array = (int*) lwrealloc(_reads_per_height->array,
                sizeof (int)*_reads_per_height->maxelements);
    }
    _reads_per_height->array[level] += incremented_v;
    if ((level + 1) > _reads_per_height->nofelements) {
        _reads_per_height->nofelements = level + 1;
    }
}

void append_rw_order(int page_num, uint8_t type, double time) {
    _rw_order->nofelements++;
    if (_rw_order->maxelements == _rw_order->nofelements) {
        _rw_order->maxelements *= 2;
        _rw_order->node = (int*) lwrealloc(_rw_order->node,
                sizeof (int)*_rw_order->maxelements);
        _rw_order->request_type = (uint8_t*) lwrealloc(_rw_order->request_type,
                sizeof (uint8_t) * _rw_order->maxelements);
        _rw_order->time = (double*) lwrealloc(_rw_order->time,
                sizeof (double) * _rw_order->maxelements);
    }
    _rw_order->node[_rw_order->nofelements - 1] = page_num;
    _rw_order->request_type[_rw_order->nofelements - 1] = type;
    _rw_order->time[_rw_order->nofelements - 1] = time;
}

void statistic_free_allocated_memory() {
    if (_writes_per_height != NULL) {
        lwfree(_writes_per_height->array);
        lwfree(_writes_per_height);
        _writes_per_height = NULL;
    }

    if (_reads_per_height != NULL) {
        lwfree(_reads_per_height->array);
        lwfree(_reads_per_height);
        _reads_per_height = NULL;
    }

    if (_rw_order != NULL) {
        lwfree(_rw_order->node);
        lwfree(_rw_order->request_type);
        lwfree(_rw_order->time);
        lwfree(_rw_order);
        _rw_order = NULL;
    }
}

void statistic_reset_variables() {
    if (is_flashdbsim_initialized) {
        /*FlashDBSim objects to get statistical information*/
        IVFD_COUNTER_t ic = f_get_vfd_counter_c();
        IVFD_LATENCY_t il = f_get_vfd_latency_c();

        f_reset_counter_c(ic);
        f_reset_latency_total_c(il);
    }

    _height = 0;

    /* variables to manage time in ms (elapsed time) */
    _total_time = 0.0; //total time to completely process an operation (e.g., a range query)
    _index_time = 0.0; //time that the spatial index took to process an operation (done)
    _filter_time = 0.0; //time for the filter step for a spatial query with index (done)
    _refinement_time = 0.0; //time for the refinement step for a spatial query with index (this indicates the time of 9-IM processing) (done)
    _retrieving_objects_time = 0.0;
    _processing_predicates_time = 0.0;
    _read_time = 0.0; //total time for all performed read operations (done)
    _write_time = 0.0; //total time for all performed write operations (done)
    _split_time = 0.0; //total time for all performed split operations (done)

    /* variables to manage time in ms (CPU time) */
    _total_cpu_time = 0.0; //total time to completely process an operation (e.g., a range query)
    _index_cpu_time = 0.0; //time that the spatial index took to return the set of candidates (done)
    _filter_cpu_time = 0.0; //time for the filter step for a spatial query with index (done)
    _refinement_cpu_time = 0.0; //time for the refinement step for a spatial query with index (this indicates the time of 9-IM processing) (done)
    _retrieving_objects_cpu_time = 0.0;
    _processing_predicates_cpu_time = 0.0;
    _read_cpu_time = 0.0; //total time for all performed read operations (done)
    _write_cpu_time = 0.0; //total time for all performed write operations (done)
    _split_cpu_time = 0.0; //total time for all performed split operations (done)

    /* variables to manage numbers/amounts */
    _cand_num = 0; //number of candidates returned by the filtering step (done)
    _result_num = 0; //number of returned spatial objects of a query (done)
    _read_num = 0; //number of read operations (done)
    _write_num = 0; //number of write operations (done)
    _split_int_num = 0; //number of split operations done in the internal nodes (done)
    _split_leaf_num = 0; //number of split operations done in the leaf nodes (done)
    _processed_entries_num = 0;
    _reinsertion_num = 0;
    _visited_int_node_num = 0; //number of visited internal nodes in an operation --this is to ACCESS a node (done)
    _visited_leaf_node_num = 0; //number of visited leaf nodes in an operation --this is to ACCESS a node (done)
    _written_int_node_num = 0; //number of written internal nodes in an operation --this is to WRITE a node (done)
    _written_leaf_node_num = 0; //number of written leaf nodes in an operation --this is to WRITE a node (done)
    _deleted_int_node_num = 0; //number of deleted internal nodes in an operation --this is to REMOVE a node (done)
    _deleted_leaf_node_num = 0; //number of deleted leaf nodes in an operation --this is to REMOVE a node (done)

    _entries_int_nodes = 0; //number of occupied entries in internal nodes
    _entries_leaf_nodes = 0; //number of occupied entries in leaf nodes
    _internal_nodes_num = 0; //number of the created internal nodes
    _leafs_nodes_num = 0; //number of the created leaf nodes

    _flushing_time = 0.0; //time to flush a part of buffer to ssd
    _flushing_cpu_time = 0.0; //the same of previous but for cpu time
    _flushing_num = 0; //number of performed flushing
    _flushed_nodes_num = 0;
    _nof_unnecessary_flushed_nodes = 0;
    //total values
    _mod_node_buffer_num = 0; //TOTAL number of modified entries that passed in the buffer
    _new_node_buffer_num = 0; //TOTAL number of newly created nodes that was stored in the buffer
    _del_node_buffer_num = 0; //TOTAL number of removed nodes that was stored in the buffer
    //current values -> we cant reset these variables since it show the current values of the buffer
    //_cur_mod_node_buffer_num = 0; //number of modified entries in the buffer
    //_cur_new_node_buffer_num = 0; //number of newly created nodes in the buffer
    //_cur_del_node_buffer_num = 0; //number of removed nodes in the buffer
    //_cur_buffer_size = 0; //the size in bytes of the buffer after an operation
    _ret_node_from_buf_time = 0.0; //total time that was waste to retrieve nodes from the buffer
    _ret_node_from_buf_cpu_time = 0.0; //total time that was waste to retrieve nodes from the buffer (cpu time))
    _write_log_time = 0.0; //total time to manage the writes performed on the log 
    _write_log_cpu_time = 0.0; //total CPU time to manage the writes performed on the log
    _compactation_log_time = 0.0; //time to compact the log (for durability)
    _compactation_log_cpu_time = 0.0; //the same of previous but for cpu time
    _recovery_log_time = 0.0; //time to recovery the buffer in main memory from log (for durability)
    _recovery_log_cpu_time = 0.0; //the same of previous but for cpu time
    _compactation_log_num = 0; //number of times that the compaction of log was done
    _write_log_num = 0; //number of writes in the log (to guarantee durability)
    _read_log_num = 0;
    _cur_log_size = 0; //the size in bytes of the log file after an operation

    _int_o_nodes_num = 0;
    _merge_back_num = 0;
    _leaf_o_nodes_num = 0;
    _entries_int_o_nodes = 0; //number of entries in internal o-nodes
    _entries_leaf_o_nodes = 0; //number of entries in leaf o-nodes

    _sbuffer_find_cpu_time = 0.0;
    _sbuffer_find_time = 0.0;
    _sbuffer_flushing_cpu_time = 0.0;
    _sbuffer_flushing_time = 0.0;
    _sbuffer_page_fault = 0;
    _sbuffer_page_hit = 0;

    /* statistical values for efind indices*/
    _read_buffer_page_hit = 0; //number of hits done by the read buffer of the efind
    _read_buffer_page_fault = 0; //number of faults done by the read buffer of the efind
    //_cur_read_buffer_size = 0; //read buffer size of the efind
    _read_buffer_put_node_cpu_time = 0.0; //consuming time to put a node in the read buffer
    _read_buffer_put_node_time = 0.0; //consuming time to put a node in the read buffer
    _read_buffer_get_node_cpu_time = 0.0; //consuming time to get a node from the read buffer
    _read_buffer_get_node_time = 0.0; //consuming time to get a node from the read buffer
    _efind_force_node_in_read_buffer = 0; //the number of times that the temporal control for reads was performed
    _efind_write_temporal_control_sequential = 0; //the number of that the temporal control for writes was performed (sequential version)
    _efind_write_temporal_control_stride = 0; //the number of that the temporal control for writes was performed (stride version)
    _efind_write_temporal_control_seqstride = 0; //the number of that the temporal control for writes was performed (mixed version)
    _efind_write_temporal_control_filled = 0; //the number of that the temporal control for writes had to be completed with random nodes
}

static ArrayNode *create_arraynode(void);
static void insert_arraynode(ArrayNode *array, NodeInfo *new);
static NodeInfo *create_nodeinfo(int level, int id, double db_value, int int_value);
static void statistic_free_snapshot(void);

void statistic_free_snapshot() {
    int i;

    if (_entries_per_node != NULL) {
        for (i = 0; i < _entries_per_node->nofelements; i++) {
            lwfree(_entries_per_node->array[i]);
        }

        lwfree(_entries_per_node->array);
        lwfree(_entries_per_node);
        _entries_per_node = NULL;
    }

    if (_area_per_node != NULL) {
        for (i = 0; i < _area_per_node->nofelements; i++) {
            lwfree(_area_per_node->array[i]);
        }

        lwfree(_area_per_node->array);
        lwfree(_area_per_node);
        _area_per_node = NULL;
    }

    if (_ovp_area_per_node != NULL) {
        for (i = 0; i < _ovp_area_per_node->nofelements; i++) {
            lwfree(_ovp_area_per_node->array[i]);
        }

        lwfree(_ovp_area_per_node->array);
        lwfree(_ovp_area_per_node);
        _ovp_area_per_node = NULL;
    }

    if (_dead_space_per_node != NULL) {
        for (i = 0; i < _dead_space_per_node->nofelements; i++) {
            lwfree(_dead_space_per_node->array[i]);
        }

        lwfree(_dead_space_per_node->array);
        lwfree(_dead_space_per_node);
        _dead_space_per_node = NULL;
    }

    if (_nodes_per_level != NULL) {
        lwfree(_nodes_per_level->array);
        lwfree(_nodes_per_level);
        _nodes_per_level = NULL;
    }
}

ArrayNode *create_arraynode() {
    ArrayNode *ret;
    ret = (ArrayNode*) lwalloc(sizeof (ArrayNode));
    ret->maxelements = 500;
    ret->nofelements = 0;
    ret->array = (NodeInfo**) lwalloc(sizeof (NodeInfo*) * 500);
    return ret;
}

void insert_arraynode(ArrayNode* array, NodeInfo* new) {
    /* we need to realloc more space */
    if (array->maxelements < array->nofelements + 1) {
        array->maxelements *= 2;
        array->array = (NodeInfo**) lwrealloc(array->array, array->maxelements * sizeof (NodeInfo*));
    }

    array->array[array->nofelements] = new;
    array->nofelements++;
}

NodeInfo *create_nodeinfo(int level, int id, double db_value, int int_value) {
    NodeInfo *ret;
    ret = (NodeInfo*) lwalloc(sizeof (NodeInfo));
    ret->db_value = db_value;
    ret->id = id;
    ret->level = level;
    ret->int_value = int_value;
    return ret;
}

/*these function get data from postgres and they are now deprecated
static int insert_statistic_source(void);
static int insert_statistic_basicconfig(void);
static int insert_statistic_specializedconfig(void);
static int insert_statistic_bufferconfig(void); */
/*these function insert data into the postgres*/
static void insert_printindex(int execution_id, int nodeid, const BBox *bbox,
        int elem_position, uint8_t o_node, int node_height, hilbert_value_t hv, int parent_node,
        uint8_t variant, const char *statistic_file);
static int insert_statistic_indexconfig(const SpatialIndex *si);
static int insert_statistic_spatialindex(const SpatialIndex *si, int config_id);
static int insert_execution(const SpatialIndex *si, int idx_id, uint8_t variant, const char *statistic_file);
static void insert_snapshot(int execution_id, uint8_t variant, const char *statistic_file);
static void process_readwrite_order(int execution_id, uint8_t variant, const char *statistic_file);
static void insert_readwrite_order(int execution_id, char *op_type, double time, int node, uint8_t variant, const char *statistic_file);
static void insert_flashsimulator_statistics(int execution_id, uint8_t variant, const char *statistic_file);

/* these functions are now deprecated because we are now assuming that
 * src_id, bc_id, sc_id, and buf_id are not known.
 * however, in the future, we can reuse these functions
 * hence, we will only comment it
int insert_statistic_source() {
    int err;
    int src_id;
    char select[512];

    sprintf(select, "SELECT src_id FROM fds.source WHERE schema_name = '%s'"
            " AND table_name = '%s' AND column_name = '%s' AND pk_name='%s';",
            _schema_name, _table_name, _column_name, _pk_name);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "insert_source: could not connect to SPI manager");
        return -1;
    }

    err = SPI_execute(select, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "insert_source: could not execute the SELECT command");
        return -1;
    }

    //there is no source, therefore, we have to insert it
    if (SPI_processed <= 0) {
        char insert[512];

        sprintf(insert, "INSERT INTO fds.source(schema_name, table_name, column_name, pk_name) "
                "VALUES ('%s', '%s', '%s', '%s') RETURNING src_id;", _schema_name, _table_name, _column_name, _pk_name);
        err = SPI_execute(insert, false, 1);
        if (err < 0) {
            SPI_finish();
            _DEBUG(ERROR, "insert_source: could not execute the INSERT command");
            return -1;
        }
    }
    src_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));

    SPI_finish();

    return src_id;
}

int insert_statistic_basicconfig() {
    int err;
    int bc_id;
    char select[936];
    char *ioaccess;
    char *refintype;

    if (_io_access == DIRECT_ACCESS) {
        ioaccess = lwalloc(sizeof ("DIRECT ACCESS"));
        sprintf(ioaccess, "DIRECT ACCESS");
    } else {
        ioaccess = lwalloc(sizeof ("NORMAL ACCESS"));
        sprintf(ioaccess, "NORMAL ACCESS");
    }

    if (_refinement_type == ONLY_GEOS) {
        refintype = lwalloc(sizeof ("ONLY GEOS"));
        sprintf(refintype, "ONLY GEOS");
    } else if (_refinement_type == GEOS_AND_POINT_POLYGON) {
        refintype = lwalloc(sizeof ("GEOS AND POINT POLYGON CHECK FROM POSTGIS"));
        sprintf(refintype, "GEOS AND POINT POLYGON CHECK FROM POSTGIS");
    }
    sprintf(select, "SELECT bc_id FROM fds.basicconfiguration "
            "WHERE page_size = %d AND ss_id = %d "
            " AND io_access = '%s' AND refinement_type = '%s';",
            page_size, _ss_id, ioaccess, refintype);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "insert_basicconfig: could not connect to SPI manager");
        return -1;
    }
    err = SPI_execute(select, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "insert_basicconfig: could not execute the SELECT command");
        return -1;
    }

    //there is no source, therefore, we have to insert it
    if (SPI_processed <= 0) {
        char insert[936];

        sprintf(insert, "INSERT INTO fds.basicconfiguration(page_size, "
                "ss_id, io_access, refinement_type) VALUES (%d, "
                "%d, '%s', '%s') RETURNING bc_id;",
                page_size, _ss_id, ioaccess, refintype);
        err = SPI_execute(insert, false, 1);
        if (err < 0) {
            SPI_finish();
            _DEBUG(ERROR, "insert_basicconfig: could not execute the INSERT command");
            return -1;
        }
    }
    bc_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));

    SPI_finish();

    lwfree(ioaccess);
    lwfree(refintype);

    return bc_id;
}

int insert_statistic_bufferconfig() {
    int err;
    int buf_id;
    char select[936];
    char *buf_type;

    if (_buffer_type == BUFFER_NONE) {
        buf_type = lwalloc(sizeof ("NONE"));
        sprintf(buf_type, "NONE");
    } else if (_buffer_type == BUFFER_LRU) {
        buf_type = lwalloc(sizeof ("LRU"));
        sprintf(buf_type, "LRU");
    } else if (_buffer_type == BUFFER_HLRU) {
        buf_type = lwalloc(sizeof ("HLRU"));
        sprintf(buf_type, "HLRU");
    }

    sprintf(select, "SELECT buf_id FROM fds.bufferconfiguration "
            "WHERE buf_type = '%s' AND min_size = %d "
            " AND max_size = %d;",
            buf_type, _buffer_min_size, _buffer_max_size);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "insert_buffconfig: could not connect to SPI manager");
        return -1;
    }
    err = SPI_execute(select, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "insert_buffconfig: could not execute the SELECT command");
        return -1;
    }

    //there is no source, therefore, we have to insert it
    if (SPI_processed <= 0) {
        char insert[936];

        sprintf(insert, "INSERT INTO fds.bufferconfiguration(buf_type, "
                "min_size, max_size) VALUES ('%s', "
                "%d, %d) RETURNING buf_id;",
                buf_type, _buffer_min_size, _buffer_max_size);
        err = SPI_execute(insert, false, 1);
        if (err < 0) {
            SPI_finish();
            _DEBUG(ERROR, "insert_buffconfig: could not execute the INSERT command");
            return -1;
        }
    }
    buf_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));

    SPI_finish();

    lwfree(buf_type);

    return buf_id;
}

int insert_statistic_specializedconfig() {
    int err;
    int sc_id;
    char select[1024];
    char *split = NULL;
    char *rt = NULL;
    char *fp = NULL;
    char *tc = NULL;

    if (_index_type == CONVENTIONAL_RTREE) {
        if (_split_type == RTREE_LINEAR_SPLIT) {
            split = lwalloc(sizeof ("LINEAR"));
            sprintf(split, "LINEAR");
        } else if (_split_type == RTREE_EXPONENTIAL_SPLIT) {
            split = lwalloc(sizeof ("EXPONENTIAL"));
            sprintf(split, "EXPONENTIAL");
        } else if (_split_type == RTREE_QUADRATIC_SPLIT) {
            split = lwalloc(sizeof ("QUADRATIC"));
            sprintf(split, "QUADRATIC");
        } else if (_split_type == RSTARTREE_SPLIT) {
            split = lwalloc(sizeof ("RSTARTREE SPLIT"));
            sprintf(split, "RSTARTREE SPLIT");
        }

        sprintf(select, "SELECT sc_id FROM fds.rtreeconfiguration WHERE split_type = '%s'"
                " AND or_id = %d;",
                split, _or_id);
    } else if (_index_type == CONVENTIONAL_RSTARTREE) {
        if (_reinsertion_type == FAR_REINSERT) {
            rt = lwalloc(sizeof ("FAR REINSERT"));
            sprintf(rt, "FAR REINSERT");
        } else {
            rt = lwalloc(sizeof ("CLOSE REINSERT"));
            sprintf(rt, "CLOSE REINSERT");
        }

        sprintf(select, "SELECT sc_id FROM fds.rstartreeconfiguration WHERE reinsertion_type = '%s' "
                "AND reinsertion_perc_internal_node = %.17g AND reinsertion_perc_leaf_node = %.17g "
                "AND max_neighbors_exam = %d AND or_id = %d;",
                rt, _reinsert_percentage_internal_node, _reinsert_percentage_leaf_node, _max_neighbors_exam, _or_id);
    } else if (_index_type == FAST_RSTARTREE_TYPE || _index_type == FAST_RTREE_TYPE) {
        char *ix;
        if (_fast_flushing_policy == FLUSH_ALL) {
            fp = lwalloc(sizeof ("FLUSH ALL"));
            sprintf(fp, "FLUSH ALL");
        } else if (_fast_flushing_policy == RANDOM_FLUSH) {
            fp = lwalloc(sizeof ("RANDOM FLUSH"));
            sprintf(fp, "RANDOM FLUSH");
        } else if (_fast_flushing_policy == FAST_FLUSHING_POLICY) {
            fp = lwalloc(sizeof ("FAST FLUSHING POLICY"));
            sprintf(fp, "FAST FLUSHING POLICY");
        } else if (_fast_flushing_policy == FAST_STAR_FLUSHING_POLICY) {
            fp = lwalloc(sizeof ("FAST STAR FLUSHING POLICY"));
            sprintf(fp, "FAST STAR FLUSHING POLICY");
        }

        if (_index_type == FAST_RTREE_TYPE) {
            ix = lwalloc(sizeof ("RTREE"));
            sprintf(ix, "RTREE");
        } else if (_index_type == FAST_RSTARTREE_TYPE) {
            ix = lwalloc(sizeof ("RSTARTREE"));
            sprintf(ix, "RSTARTREE");
        }

        sprintf(select, "SELECT sc_id FROM fds.fastconfiguration WHERE index_type = '%s' AND "
                "db_sc_id = %d AND buffer_size = %d "
                "AND flushing_unit_size = %d AND flushing_policy = '%s' AND log_size = %d;",
                ix, _fast_sc_id, _fast_buffer_size, _fast_flushing_size, fp, _fast_log_size);
        lwfree(ix);
    } else if (_index_type == FORTREE_TYPE) {
        sprintf(select, "SELECT sc_id FROM fds.fortreeconfiguration WHERE buffer_size = %d AND "
                "flushing_unit_size = %d AND ratio_flushing = %d "
                "AND x = %.17g AND y = %.17g AND or_id = %d;",
                _for_buffer_size, _for_flushing_size, _for_ratio_flushing, _for_x, _for_y, _or_id);
    } else if (_index_type == eFIND_RSTARTREE_TYPE || _index_type == eFIND_RTREE_TYPE) {
        char *ix;

        if (_efind_flushing_policy == eFIND_M_FP) {
            fp = lwalloc(sizeof ("eFIND FLUSH MOD"));
            sprintf(fp, "eFIND FLUSH MOD");
        } else if (_efind_flushing_policy == eFIND_MT_FP) {
            fp = lwalloc(sizeof ("eFIND FLUSH MOD TIME"));
            sprintf(fp, "eFIND FLUSH MOD TIME");
        } else if (_efind_flushing_policy == eFIND_MTH_FP) {
            fp = lwalloc(sizeof ("eFIND FLUSH MOD TIME HEIGHT"));
            sprintf(fp, "eFIND FLUSH MOD TIME HEIGHT");
        } else if (_efind_flushing_policy == eFIND_MTHA_FP) {
            fp = lwalloc(sizeof ("eFIND FLUSH MOD TIME HEIGHT AREA"));
            sprintf(fp, "eFIND FLUSH MOD TIME HEIGHT AREA");
        } else if (_efind_flushing_policy == eFIND_MTHAO_FP) {
            fp = lwalloc(sizeof ("eFIND FLUSH MOD TIME HEIGHT AREA OVERLAP"));
            sprintf(fp, "eFIND FLUSH MOD TIME HEIGHT AREA OVERLAP");
        }

        if (_efind_temporal_control_policy == eFIND_NONE_TCP) {
            tc = lwalloc(sizeof ("TEMPORAL CONTROL NONE"));
            sprintf(tc, "TEMPORAL CONTROL NONE");
        } else if (_efind_temporal_control_policy == eFIND_READ_TCP) {
            tc = lwalloc(sizeof ("TEMPORAL CONTROL FOR READS"));
            sprintf(tc, "TEMPORAL CONTROL FOR READS");
        } else if (_efind_temporal_control_policy == eFIND_WRITE_TCP) {
            tc = lwalloc(sizeof ("TEMPORAL CONTROL FOR WRITES"));
            sprintf(tc, "TEMPORAL CONTROL FOR WRITES");
        } else if (_efind_temporal_control_policy == eFIND_READ_WRITE_TCP) {
            tc = lwalloc(sizeof ("TEMPORAL CONTROL FOR READS AND WRITES"));
            sprintf(tc, "TEMPORAL CONTROL FOR READS AND WRITES");
        }

        if (_index_type == eFIND_RTREE_TYPE) {
            ix = lwalloc(sizeof ("RTREE"));
            sprintf(ix, "RTREE");
        } else if (_index_type == eFIND_RSTARTREE_TYPE) {
            ix = lwalloc(sizeof ("RSTARTREE"));
            sprintf(ix, "RSTARTREE");
        }

        sprintf(select, "SELECT sc_id FROM fds.efindconfiguration WHERE index_type = '%s' AND "
                "db_sc_id = %d AND buffer_size = %d AND read_buffer_perc = %.17g "
                "AND temporal_control_policy = '%s' AND read_temporal_control_size = %d AND "
                "read_temporal_control_threshold = %.17g AND write_temporal_control_size = %d AND "
                "write_temporal_control_mindist = %d AND write_temporal_control_stride = %d AND "
                "flushing_percentage = %.17g "
                "AND flushing_unit_size = %d AND flushing_policy = '%s' AND log_size = %d;",
                ix, _efind_sc_id, _efind_read_buffer_size + _efind_write_buffer_size, _efind_read_buffer_perc,
                tc, _efind_read_tc_size, _efind_read_tc_threshold, _efind_write_tc_size,
                _efind_write_tc_mindist, _efind_write_tc_stride,
                _efind_flushing_perc,
                _efind_flushing_unit_size, fp, _efind_log_size);
        lwfree(ix);
    }

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "insert_specializedconfiguration: could not connect to SPI manager");
        return -1;
    }
    err = SPI_execute(select, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "insert_specializedconfiguration: could not execute the SELECT command");
        return -1;
    }

    //there is no source, therefore, we have to insert it
    if (SPI_processed <= 0) {
        _DEBUG(ERROR, "Statistical data processing error: there is not the specialized configuration.")
    }
    sc_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));

    SPI_finish();

    if (split != NULL)
        lwfree(split);
    if (rt != NULL)
        lwfree(rt);
    if (fp != NULL)
        lwfree(fp);
    if (tc != NULL)
        lwfree(tc);

    return sc_id;
} */

int insert_statistic_indexconfig(const SpatialIndex *si) {
    int err;
    int config_id;
    char select[256];

    sprintf(select, "SELECT config_id FROM fds.indexconfiguration "
            "WHERE sc_id = %d AND src_id = %d AND bc_id = %d AND buf_id = %d;",
            si->sc_id, si->src->src_id, si->gp->bc_id, si->bs->buf_id);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "insert_indexconfiguration: could not connect to SPI manager");
        return -1;
    }
    err = SPI_execute(select, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "insert_indexconfiguration: could not execute the SELECT command");
        return -1;
    }

    if (SPI_processed <= 0) {
        char insert[256];

        sprintf(insert, "INSERT INTO "
                "fds.indexconfiguration(sc_id, src_id, bc_id, buf_id) "
                "VALUES (%d, %d, %d, %d) RETURNING config_id;",
                si->sc_id, si->src->src_id, si->gp->bc_id, si->bs->buf_id);
        err = SPI_execute(insert, false, 1);
        if (err < 0) {
            SPI_finish();
            _DEBUG(ERROR, "insert_indexconfiguration: could not execute the INSERT command");
            return -1;
        }
    }
    config_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));

    SPI_finish();

    return config_id;
}

int insert_statistic_spatialindex(const SpatialIndex *si, int config_id) {
    int err;
    int idx_id;
    char select[256];
    char update[256];
    char *idx_name;
    char *idx_path;

    char *aux = strdup(si->index_file);
    char *aux2 = strdup(si->index_file);

    idx_name = basename(aux);
    idx_path = dirname(aux2);

    sprintf(select, "SELECT idx_id FROM fds.spatialindex WHERE config_id = %d"
            " AND idx_name = '%s' AND idx_path = '%s/';",
            config_id, idx_name, idx_path);

    if (SPI_OK_CONNECT != SPI_connect()) {
        SPI_finish();
        _DEBUG(ERROR, "insert_spatialindex: could not connect to SPI manager");
        return -1;
    }
    err = SPI_execute(select, true, 1);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "insert_spatialindex: could not execute the SELECT command");
        return -1;
    }

    if (SPI_processed <= 0) {
        char insert[256];

        sprintf(insert, "INSERT INTO fds.spatialindex(config_id, idx_name, idx_path, "
                "idx_creation, idx_last_mod) "
                "VALUES (%d, '%s', '%s/', now(), now()) RETURNING idx_id;",
                config_id, idx_name, idx_path);
        err = SPI_execute(insert, false, 1);
        if (err < 0) {
            SPI_finish();
            _DEBUG(ERROR, "insert_spatialindex: could not execute the INSERT command");
            return -1;
        }
    }
    idx_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));

    //now we have to update the last_mod
    sprintf(update, "UPDATE fds.spatialindex SET idx_last_mod = now() WHERE idx_id = %d",
            idx_id);
    err = SPI_execute(update, false, 0);
    if (err < 0) {
        SPI_finish();
        _DEBUG(ERROR, "insert_spatialindex: could not execute the UPDATE command");
        return -1;
    }

    //note that this is the common free!!! because of the strdup
    free(aux);
    free(aux2);

    SPI_finish();

    return idx_id;
}

int insert_execution(const SpatialIndex *si, int idx_id, uint8_t variant, const char *statistic_file) {
    int err;
    stringbuffer_t *sb;
    const char *query;
    int pe_id = -1;
    int i;
    int n;

    sb = stringbuffer_create();

    stringbuffer_append(sb, "INSERT INTO fds.execution(");    
    stringbuffer_append(sb, "idx_id, ");
    if(_execution_name != NULL) {
        stringbuffer_append(sb, "execution_name, ");
    }
    stringbuffer_append(sb, "total_time, ");
    stringbuffer_append(sb, "index_time, ");
    stringbuffer_append(sb, "filter_time, ");
    stringbuffer_append(sb, "refinement_time, ");
    stringbuffer_append(sb, "retrieving_objects_time, ");
    stringbuffer_append(sb, "processing_predicates_time, ");
    stringbuffer_append(sb, "read_time, ");
    stringbuffer_append(sb, "write_time, ");
    stringbuffer_append(sb, "split_time, ");
    stringbuffer_append(sb, "total_cpu_time, ");
    stringbuffer_append(sb, "index_cpu_time, ");
    stringbuffer_append(sb, "filter_cpu_time, ");
    stringbuffer_append(sb, "refinement_cpu_time, ");
    stringbuffer_append(sb, "retrieving_objects_cpu_time, ");
    stringbuffer_append(sb, "processing_predicates_cpu_time, ");
    stringbuffer_append(sb, "read_cpu_time, ");
    stringbuffer_append(sb, "write_cpu_time, ");
    stringbuffer_append(sb, "split_cpu_time, ");
    stringbuffer_append(sb, "processed_entries_num, ");
    stringbuffer_append(sb, "reinsertion_num, ");
    stringbuffer_append(sb, "cand_num, ");
    stringbuffer_append(sb, "result_num, ");
    stringbuffer_append(sb, "reads_num, ");
    stringbuffer_append(sb, "writes_num, ");
    stringbuffer_append(sb, "split_int_num, ");
    stringbuffer_append(sb, "split_leaf_num, ");
    stringbuffer_append(sb, "visited_leaf_nodes_num, ");
    stringbuffer_append(sb, "visited_int_nodes_num, ");
    stringbuffer_append(sb, "query_predicate, ");
    stringbuffer_append(sb, "flushing_time, ");
    stringbuffer_append(sb, "flushing_cpu_time, ");
    stringbuffer_append(sb, "flushing_num, ");
    stringbuffer_append(sb, "nof_unnecessary_flushed_nodes, ");
    stringbuffer_append(sb, "written_int_nodes_num, ");
    stringbuffer_append(sb, "written_leaf_nodes_num, ");
    stringbuffer_append(sb, "deleted_int_nodes_num, ");
    stringbuffer_append(sb, "deleted_leaf_nodes_num, ");
    stringbuffer_append(sb, "mod_node_buffer_num, ");
    stringbuffer_append(sb, "new_node_buffer_num, ");
    stringbuffer_append(sb, "del_node_buffer_num, ");
    stringbuffer_append(sb, "cur_mod_node_buffer_num, ");
    stringbuffer_append(sb, "cur_new_node_buffer_num, ");
    stringbuffer_append(sb, "cur_del_node_buffer_num, ");
    stringbuffer_append(sb, "cur_buffer_size, ");
    stringbuffer_append(sb, "ret_node_from_buf_time, ");
    stringbuffer_append(sb, "ret_node_from_buf_cpu_time, ");
    stringbuffer_append(sb, "write_log_time, ");
    stringbuffer_append(sb, "write_log_cpu_time, ");
    stringbuffer_append(sb, "compaction_log_time, ");
    stringbuffer_append(sb, "compaction_log_cpu_time, ");
    stringbuffer_append(sb, "recovery_log_time, ");
    stringbuffer_append(sb, "recovery_log_cpu_time, ");
    stringbuffer_append(sb, "compaction_log_num, ");
    stringbuffer_append(sb, "writes_log_num, ");
    stringbuffer_append(sb, "cur_log_size, ");
    stringbuffer_append(sb, "flushed_nodes_num, ");
    stringbuffer_append(sb, "merge_back_num, ");
    stringbuffer_append(sb, "mods_pheight, ");
    stringbuffer_append(sb, "accesses_pheight, ");
    stringbuffer_append(sb, "std_buffer_page_fault, ");
    stringbuffer_append(sb, "std_buffer_page_hit, ");
    stringbuffer_append(sb, "std_buffer_find_time, ");
    stringbuffer_append(sb, "std_buffer_find_cpu_time, ");
    stringbuffer_append(sb, "std_buffer_flushing_time, ");
    stringbuffer_append(sb, "std_buffer_flushing_cpu_time, ");
    stringbuffer_append(sb, "read_buffer_page_hit, ");
    stringbuffer_append(sb, "read_buffer_page_fault, ");
    stringbuffer_append(sb, "cur_read_buffer_size, ");
    stringbuffer_append(sb, "read_buffer_put_node_cpu_time, ");
    stringbuffer_append(sb, "read_buffer_put_node_time, ");
    stringbuffer_append(sb, "read_buffer_get_node_cpu_time, ");
    stringbuffer_append(sb, "read_buffer_get_node_time, ");
    stringbuffer_append(sb, "efind_force_node_in_rbuffer, ");
    stringbuffer_append(sb, "efind_write_tc_sequential, ");
    stringbuffer_append(sb, "efind_write_tc_stride, ");
    stringbuffer_append(sb, "efind_write_tc_seqstride, ");
    stringbuffer_append(sb, "efind_write_tc_filled");

    stringbuffer_append(sb, ") VALUES (");

    //idx_id
    if (variant & SO_STORE_STATISTICAL_IN_FILE) {
        char *idx_name;
        char *idx_path;

        char *aux = strdup(si->index_file);
        char *aux2 = strdup(si->index_file);

        idx_name = basename(aux);
        idx_path = dirname(aux2);
        //if we will store in a SQL file, we use the SQL function to get the idx_id
        stringbuffer_aprintf(sb, "_FT_ProcessStatisticSpatialIndex('%s', '%s/', %d, %d, %d, %d), ",
                idx_name, idx_path, si->src->src_id, si->gp->bc_id, si->sc_id, si->bs->buf_id);
        
        free(aux); //note that this is the common free!!! because of the strdup
        free(aux2);
    } else {
        //if we will store in the FESTIval's data schema, then we already have the idx_id        
        stringbuffer_aprintf(sb, "%d, ", idx_id);
    }
    
    //execution_name
    if(_execution_name != NULL) {
        stringbuffer_aprintf(sb, "'%s', ", _execution_name);
    }
    //printf("%.17g", number) --extracts all the digits of a double value!

    //total_time    
    stringbuffer_aprintf(sb, "%.17g, ", _total_time);
    //index_time
    stringbuffer_aprintf(sb, "%.17g, ", _index_time);
    //filter_time
    stringbuffer_aprintf(sb, "%.17g, ", _filter_time);
    //refinement_time
    stringbuffer_aprintf(sb, "%.17g, ", _refinement_time);
    //retrieving_objects_time
    stringbuffer_aprintf(sb, "%.17g, ", _retrieving_objects_time);
    //processing_predicates_time
    stringbuffer_aprintf(sb, "%.17g, ", _processing_predicates_time);
    //read_time
    stringbuffer_aprintf(sb, "%.17g, ", _read_time);
    //write_time
    stringbuffer_aprintf(sb, "%.17g, ", _write_time);
    //split_time
    stringbuffer_aprintf(sb, "%.17g, ", _split_time);
    //total_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _total_cpu_time);
    //index_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _index_cpu_time);
    //filter_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _filter_cpu_time);
    //refinement_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _refinement_cpu_time);
    //retrieving_objects_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _retrieving_objects_cpu_time);
    //processing_predicates_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _processing_predicates_cpu_time);
    //read_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _read_cpu_time);
    //write_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _write_cpu_time);
    //split_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _split_cpu_time);
    //processed_entries_num
    stringbuffer_aprintf(sb, "%llu, ", _processed_entries_num);
    //_reinsertion_num
    stringbuffer_aprintf(sb, "%d, ", _reinsertion_num);
    //can_num
    stringbuffer_aprintf(sb, "%d, ", _cand_num);
    //result_num
    stringbuffer_aprintf(sb, "%d, ", _result_num);
    //reads_num
    stringbuffer_aprintf(sb, "%d, ", _read_num);
    //write_num
    stringbuffer_aprintf(sb, "%d, ", _write_num);
    //split_int_num
    stringbuffer_aprintf(sb, "%d, ", _split_int_num);
    //split_leaf_num
    stringbuffer_aprintf(sb, "%d, ", _split_leaf_num);
    //visited_leaf_node_num
    stringbuffer_aprintf(sb, "%d, ", _visited_leaf_node_num);
    //visited_int_node_num
    stringbuffer_aprintf(sb, "%d, ", _visited_int_node_num);
    //query_predicate
    stringbuffer_append(sb, "'");
    switch (_query_predicate) {
        case INTERSECTS:
            stringbuffer_append(sb, "INTERSECTS");
            break;
        case OVERLAP:
            stringbuffer_append(sb, "OVERLAP");
            break;
        case DISJOINT:
            stringbuffer_append(sb, "DISJOINT");
            break;
        case EQUAL:
            stringbuffer_append(sb, "EQUAL");
            break;
        case MEET:
            stringbuffer_append(sb, "MEET");
            break;
        case INSIDE:
            stringbuffer_append(sb, "INSIDE");
            break;
        case COVEREDBY:
            stringbuffer_append(sb, "COVEREDBY");
            break;
        case CONTAINS:
            stringbuffer_append(sb, "CONTAINS");
            break;
        case COVERS:
            stringbuffer_append(sb, "COVERS");
            break;
        default:
            stringbuffer_append(sb, "NO PREDICATE");
            break;
    }
    stringbuffer_append(sb, "', ");
    //flushing_time
    stringbuffer_aprintf(sb, "%.17g, ", _flushing_time);
    //flushing_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _flushing_cpu_time);
    //flushing_num
    stringbuffer_aprintf(sb, "%d, ", _flushing_num);
    //nof_unnecessary_flushed_nodes
    stringbuffer_aprintf(sb, "%d, ", _nof_unnecessary_flushed_nodes);
    //written_int_node_num
    stringbuffer_aprintf(sb, "%d, ", _written_int_node_num);
    //written_leaf_node_num
    stringbuffer_aprintf(sb, "%d, ", _written_leaf_node_num);
    //deleted_int_node_num
    stringbuffer_aprintf(sb, "%d, ", _deleted_int_node_num);
    //deleted_leaf_node_num
    stringbuffer_aprintf(sb, "%d, ", _deleted_leaf_node_num);
    //mod_node_buffer_num
    stringbuffer_aprintf(sb, "%d, ", _mod_node_buffer_num);
    //new_node_buffer_num
    stringbuffer_aprintf(sb, "%d, ", _new_node_buffer_num);
    //del_node_buffer_num
    stringbuffer_aprintf(sb, "%d, ", _del_node_buffer_num);
    //cur_mod_node_buffer_num
    stringbuffer_aprintf(sb, "%d, ", _cur_mod_node_buffer_num);
    //cur_new_node_buffer_num
    stringbuffer_aprintf(sb, "%d, ", _cur_new_node_buffer_num);
    //cur_del_node_buffer_num
    stringbuffer_aprintf(sb, "%d, ", _cur_del_node_buffer_num);
    //cur_buffer_size
    stringbuffer_aprintf(sb, "%d, ", _cur_buffer_size);
    //ret_node_from_buf_time
    stringbuffer_aprintf(sb, "%.17g, ", _ret_node_from_buf_time);
    //ret_node_from_buf_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _ret_node_from_buf_cpu_time);
    //_write_log_time
    stringbuffer_aprintf(sb, "%.17g, ", _write_log_time);
    //_write_log_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _write_log_cpu_time);
    //compaction_log_time
    stringbuffer_aprintf(sb, "%.17g, ", _compactation_log_time);
    //compaction_log_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _compactation_log_cpu_time);
    //recovery_log_time
    stringbuffer_aprintf(sb, "%.17g, ", _recovery_log_time);
    //recovery_log_cpu_time
    stringbuffer_aprintf(sb, "%.17g, ", _recovery_log_cpu_time);
    //compaction_log_num
    stringbuffer_aprintf(sb, "%d, ", _compactation_log_num);
    //write_log_num
    stringbuffer_aprintf(sb, "%d, ", _write_log_num);
    //cur_log_size
    stringbuffer_aprintf(sb, "%d, ", _cur_log_size);
    //flushed_nodes_num
    stringbuffer_aprintf(sb, "%d, ", _flushed_nodes_num);
    //_merge_back_num
    stringbuffer_aprintf(sb, "%d, ", _merge_back_num);
    //_writes_pheight
    stringbuffer_append(sb, "'[");
    if (_writes_per_height->nofelements == 0)
        n = 1;
    else
        n = _writes_per_height->nofelements;
    for (i = 0; i < n; i++) {
        if (i > 0)
            stringbuffer_append(sb, ", ");

        stringbuffer_aprintf(sb, "{\"height\": %d, \"nofwrites\": %d}", i, _writes_per_height->array[i]);
    }
    stringbuffer_append(sb, "]'::jsonb, ");

    //reads_pheight
    stringbuffer_append(sb, "'[");
    if (_reads_per_height->nofelements == 0)
        n = 1;
    else
        n = _reads_per_height->nofelements;
    for (i = 0; i < n; i++) {
        if (i > 0)
            stringbuffer_append(sb, ", ");

        stringbuffer_aprintf(sb, "{\"height\": %d, \"nofreads\": %d}", i, _reads_per_height->array[i]);
    }
    stringbuffer_append(sb, "]'::jsonb, ");

    stringbuffer_aprintf(sb, "%d, ", _sbuffer_page_fault);
    stringbuffer_aprintf(sb, "%d, ", _sbuffer_page_hit);
    stringbuffer_aprintf(sb, "%.17g, ", _sbuffer_find_time);
    stringbuffer_aprintf(sb, "%.17g, ", _sbuffer_find_cpu_time);
    stringbuffer_aprintf(sb, "%.17g, ", _sbuffer_flushing_time);
    stringbuffer_aprintf(sb, "%.17g, ", _sbuffer_flushing_cpu_time);

    stringbuffer_aprintf(sb, "%d, ", _read_buffer_page_hit);
    stringbuffer_aprintf(sb, "%d, ", _read_buffer_page_fault);
    stringbuffer_aprintf(sb, "%d, ", _cur_read_buffer_size);
    stringbuffer_aprintf(sb, "%.17g, ", _read_buffer_put_node_cpu_time);
    stringbuffer_aprintf(sb, "%.17g, ", _read_buffer_put_node_time);
    stringbuffer_aprintf(sb, "%.17g, ", _read_buffer_get_node_cpu_time);
    stringbuffer_aprintf(sb, "%.17g, ", _read_buffer_get_node_time);
    stringbuffer_aprintf(sb, "%d, ", _efind_force_node_in_read_buffer);
    stringbuffer_aprintf(sb, "%d, ", _efind_write_temporal_control_sequential);
    stringbuffer_aprintf(sb, "%d, ", _efind_write_temporal_control_stride);
    stringbuffer_aprintf(sb, "%d, ", _efind_write_temporal_control_seqstride);
    stringbuffer_aprintf(sb, "%d", _efind_write_temporal_control_filled);

    stringbuffer_append(sb, ") RETURNING pe_id");

    query = stringbuffer_getstring(sb);

    if (variant & SO_STORE_STATISTICAL_IN_FILE) {
        /*we will store this SQL command in a SQL file
        we will store the pe_id into a temporary table 
         * that will contain valid values until the end of the transaction*/
        FILE *file;

        //we open the file
        file = fopen(statistic_file, "a+");
        if (file == NULL) {
            _DEBUGF(ERROR, "The file %s cannot be opened.", statistic_file);
        } else {
            //we use Common Table Expressions to store the returning pe_id in our temporary table
            fprintf(file, "WITH insert_t AS ( %s )\n", query);
            fprintf(file, "INSERT INTO execution_id_temp(id) SELECT pe_id FROM insert_t;\n");
        }
        //we close the SQL file
        fclose(file);
    } else {
        /*we will simply execute this SQL in the FESTIval's data schema*/
        if (SPI_OK_CONNECT != SPI_connect()) {
            SPI_finish();
            _DEBUG(ERROR, "insert_execution: could not connect to SPI manager");
            return -1;
        }
        err = SPI_execute(query, false, 1);
        if (err < 0) {
            SPI_finish();
            _DEBUG(ERROR, "insert_execution: could not execute the INSERT command");
            return -1;
        }

        pe_id = atoi(SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1));

        SPI_finish();
    }

    stringbuffer_destroy(sb);

    return pe_id;
}

static void insertion_handler(const char *query, uint8_t variant, const char *statistic_file);

void insertion_handler(const char *query, uint8_t variant, const char* statistic_file) {
    if (variant & SO_STORE_STATISTICAL_IN_FILE) {
        /*we will store this SQL command in a SQL file*/
        FILE *file;
        //we open the file
        file = fopen(statistic_file, "a+");
        if (file == NULL) {
            _DEBUGF(ERROR, "The file %s cannot be opened.", statistic_file);
        } else {
            fprintf(file, "%s\n", query);
        }
        //we close the SQL file
        fclose(file);
    } else {
        int err;
        if (SPI_OK_CONNECT != SPI_connect()) {
            SPI_finish();
            _DEBUG(ERROR, "insertion_handler: could not connect to SPI manager");
            return;
        }
        err = SPI_execute(query, false, 0);
        if (err < 0) {
            SPI_finish();
            _DEBUG(ERROR, "insertion_handler: could not execute the insert command");
            return;
        }
        /* disconnect from SPI */
        SPI_finish();
    }
}

void insert_printindex(int execution_id, int nodeid, const BBox *bbox, int elem_position,
        uint8_t o_node, int node_height, hilbert_value_t hv, int parent_node, uint8_t variant, const char *statistic_file) {
    LWGEOM *lwgeom = NULL;
    char *query;
    char *wkt;
    size_t wkt_size;
    stringbuffer_t *sb;

    sb = stringbuffer_create();

    lwgeom = bbox_to_geom(bbox);
    wkt = lwgeom_to_wkt(lwgeom, WKT_EXTENDED, DBL_DIG, &wkt_size);
    lwgeom_free(lwgeom);

    if (variant & SO_STORE_STATISTICAL_IN_FILE) {
        //here we use the value stored in our temporary table
        stringbuffer_aprintf(sb, "INSERT INTO fds.printindex(pe_id, node_id, geom, elem_position, o_node, node_height, hilbert_value, parent_node) "
                "VALUES ((SELECT id FROM execution_id_temp), %d, '%s'::GEOMETRY, %d, '%d', %d, %llu, %d);",
                nodeid, wkt, elem_position, o_node, node_height, hv, parent_node);
    } else {
        stringbuffer_aprintf(sb, "INSERT INTO fds.printindex(pe_id, node_id, geom, elem_position, o_node, node_height, hilbert_value, parent_node) "
                "VALUES (%d, %d, '%s'::GEOMETRY, %d, '%d', %d, %llu, %d);",
                execution_id, nodeid, wkt, elem_position, o_node, node_height, hv, parent_node);
    }

    query = stringbuffer_getstringcopy(sb);
    stringbuffer_destroy(sb);

    insertion_handler(query, variant, statistic_file);

    lwfree(query);
    lwfree(wkt);
}

void insert_snapshot(int execution_id, uint8_t variant, const char *statistic_file) {
    stringbuffer_t *sb;
    const char *query;
    int i;

    int sum_num_entries_pnode = 0;
    double sum_coverage_area_pnode = 0.0;
    double sum_overlap_area_pnode = 0.0;
    double sum_dead_space_pnode = 0.0;

    double avg_num_entries_pnode;
    double avg_coverage_area_pnode;
    double avg_overlap_area_pnode;
    double avg_dead_space_pnode;

    sb = stringbuffer_create();

    stringbuffer_append(sb, "INSERT INTO fds.indexsnapshot(");
    stringbuffer_append(sb, "pe_id, height, ");
    stringbuffer_append(sb, "num_entries_int_nodes, ");
    stringbuffer_append(sb, "num_entries_leaf_nodes, ");
    stringbuffer_append(sb, "num_int_nodes, ");
    stringbuffer_append(sb, "num_leaf_nodes, ");
    stringbuffer_append(sb, "num_nodes_pheight, ");
    stringbuffer_append(sb, "num_entries_pnode, ");
    stringbuffer_append(sb, "coverage_area_pnode, ");
    stringbuffer_append(sb, "overlap_area_pnode, ");
    stringbuffer_append(sb, "dead_space_pnode, ");
    stringbuffer_append(sb, "avg_num_entries_pnode, ");
    stringbuffer_append(sb, "avg_coverage_area_pnode, ");
    stringbuffer_append(sb, "avg_overlap_area_pnode, ");
    stringbuffer_append(sb, "avg_dead_space_pnode, ");
    stringbuffer_append(sb, "num_int_o_nodes, ");
    stringbuffer_append(sb, "num_leaf_o_nodes, ");
    stringbuffer_append(sb, "num_entries_int_o_nodes, ");
    stringbuffer_append(sb, "num_entries_leaf_o_nodes");
    stringbuffer_append(sb, ") VALUES (");

    if (variant & SO_STORE_STATISTICAL_IN_FILE) {
        stringbuffer_append(sb, "(SELECT id FROM execution_id_temp), ");
    } else {
        stringbuffer_aprintf(sb, "%d, ", execution_id);
    }
    stringbuffer_aprintf(sb, "%d, ", _height);
    stringbuffer_aprintf(sb, "%d, ", _entries_int_nodes);
    stringbuffer_aprintf(sb, "%d, ", _entries_leaf_nodes);
    stringbuffer_aprintf(sb, "%d, ", _internal_nodes_num);
    stringbuffer_aprintf(sb, "%d, ", _leafs_nodes_num);

    stringbuffer_append(sb, "'[");
    for (i = 0; i < _nodes_per_level->nofelements; i++) {
        if (i > 0)
            stringbuffer_append(sb, ", ");

        stringbuffer_aprintf(sb, "{\"height\": %d, \"nofnodes\": %d}", i, _nodes_per_level->array[i]);
    }
    stringbuffer_append(sb, "]'::jsonb, ");

    stringbuffer_append(sb, "'[");
    for (i = 0; i < _entries_per_node->nofelements; i++) {
        if (i > 0)
            stringbuffer_append(sb, ", ");

        stringbuffer_aprintf(sb, "{\"node\": %d, \"height\": %d, \"nofentries\": %d}",
                _entries_per_node->array[i]->id, _entries_per_node->array[i]->level,
                _entries_per_node->array[i]->int_value);

        sum_num_entries_pnode += _entries_per_node->array[i]->int_value;
    }
    stringbuffer_append(sb, "]'::jsonb, ");

    stringbuffer_append(sb, "'[");
    for (i = 0; i < _area_per_node->nofelements; i++) {
        if (i > 0)
            stringbuffer_append(sb, ", ");

        stringbuffer_aprintf(sb, "{\"node\": %d, \"height\": %d, \"area\": %.17g}",
                _area_per_node->array[i]->id, _area_per_node->array[i]->level,
                _area_per_node->array[i]->db_value);
        sum_coverage_area_pnode += _area_per_node->array[i]->db_value;
    }
    stringbuffer_append(sb, "]'::jsonb, ");

    stringbuffer_append(sb, "'[");
    for (i = 0; i < _ovp_area_per_node->nofelements; i++) {
        if (i > 0)
            stringbuffer_append(sb, ", ");

        stringbuffer_aprintf(sb, "{\"node\": %d, \"height\": %d, \"overlapped_area\": %.17g}",
                _ovp_area_per_node->array[i]->id, _ovp_area_per_node->array[i]->level,
                _ovp_area_per_node->array[i]->db_value);
        sum_overlap_area_pnode += _ovp_area_per_node->array[i]->db_value;
    }
    stringbuffer_append(sb, "]'::jsonb, ");

    stringbuffer_append(sb, "'[");
    for (i = 0; i < _dead_space_per_node->nofelements; i++) {
        if (i > 0)
            stringbuffer_append(sb, ", ");

        stringbuffer_aprintf(sb, "{\"node\": %d, \"height\": %d, \"dead_space_area\": %.17g}",
                _dead_space_per_node->array[i]->id, _dead_space_per_node->array[i]->level,
                _dead_space_per_node->array[i]->db_value);
        sum_dead_space_pnode += _dead_space_per_node->array[i]->db_value;
    }
    stringbuffer_append(sb, "]'::jsonb, ");

    avg_num_entries_pnode = (double) sum_num_entries_pnode / (double) _entries_per_node->nofelements;
    avg_coverage_area_pnode = (double) sum_coverage_area_pnode / (double) _area_per_node->nofelements;
    avg_overlap_area_pnode = (double) sum_overlap_area_pnode / (double) _ovp_area_per_node->nofelements;
    avg_dead_space_pnode = (double) sum_dead_space_pnode / (double) _dead_space_per_node->nofelements;

    stringbuffer_aprintf(sb, "%.17g, ", avg_num_entries_pnode);
    stringbuffer_aprintf(sb, "%.17g, ", avg_coverage_area_pnode);
    stringbuffer_aprintf(sb, "%.17g, ", avg_overlap_area_pnode);
    stringbuffer_aprintf(sb, "%.17g, ", avg_dead_space_pnode);

    stringbuffer_aprintf(sb, "%d, ", _int_o_nodes_num);
    stringbuffer_aprintf(sb, "%d, ", _leaf_o_nodes_num);
    stringbuffer_aprintf(sb, "%d, ", _entries_int_o_nodes);
    stringbuffer_aprintf(sb, "%d", _entries_leaf_o_nodes);

    stringbuffer_append(sb, ");");

    query = stringbuffer_getstring(sb);

    insertion_handler(query, variant, statistic_file);

    stringbuffer_destroy(sb);

    statistic_free_snapshot();
}

void insert_readwrite_order(int execution_id, char *op_type, double time, int node,
        uint8_t variant, const char *statistic_file) {
    char *query;
    stringbuffer_t *sb;

    sb = stringbuffer_create();

    if (variant & SO_STORE_STATISTICAL_IN_FILE) {
        stringbuffer_aprintf(sb, "INSERT INTO fds.readwriteorder"
                "(pe_id, op_type, op_timestamp, page_id) "
                "VALUES ((SELECT id FROM execution_id_temp), '%s', to_timestamp(%.10f)::timestamp, %d);",
                op_type, time, node);
    } else {
        stringbuffer_aprintf(sb, "INSERT INTO fds.readwriteorder"
                "(pe_id, op_type, op_timestamp, page_id) "
                "VALUES (%d, '%s', to_timestamp(%.10f)::timestamp, %d);",
                execution_id, op_type, time, node);
    }

    query = stringbuffer_getstringcopy(sb);
    stringbuffer_destroy(sb);

    insertion_handler(query, variant, statistic_file);

    lwfree(query);
}

void insert_flashsimulator_statistics(int execution_id, uint8_t variant, const char *statistic_file) {
    char *query;
    stringbuffer_t *sb;
    /*FlashDBSim objects to get statistical information*/
    IVFD_COUNTER_t ic = f_get_vfd_counter_c();
    IVFD_LATENCY_t il = f_get_vfd_latency_c();

    sb = stringbuffer_create();

    if (variant & SO_STORE_STATISTICAL_IN_FILE) {
        stringbuffer_aprintf(sb, "INSERT INTO fds.FlashSimulatorStatistics"
                "(pe_id, read_count, write_count, erase_count, read_latency, write_latency, erase_latency) "
                "VALUES ((SELECT id FROM execution_id_temp), %d, %d, %d, %d, %d, %d);",
                f_get_read_count_total_c(ic),
                f_get_write_count_total_c(ic),
                f_get_erase_count_total_c(ic),
                f_get_read_latency_total_c(il),
                f_get_write_latency_total_c(il),
                f_get_erase_latency_total_c(il));
    } else {
        stringbuffer_aprintf(sb, "INSERT INTO fds.FlashSimulatorStatistics"
                "(pe_id, read_count, write_count, erase_count, read_latency, write_latency, erase_latency) "
                "VALUES (%d, %d, %d, %d, %d, %d, %d);",
                execution_id, f_get_read_count_total_c(ic),
                f_get_write_count_total_c(ic),
                f_get_erase_count_total_c(ic),
                f_get_read_latency_total_c(il),
                f_get_write_latency_total_c(il),
                f_get_erase_latency_total_c(il));
    }

    query = stringbuffer_getstringcopy(sb);
    stringbuffer_destroy(sb);

    insertion_handler(query, variant, statistic_file);

    lwfree(query);
}

void process_readwrite_order(int execution_id, uint8_t variant, const char *statistic_file) {
    if (_rw_order->nofelements > 0) {
        int i;
        int n = _rw_order->nofelements;
        for (i = 0; i < n; i++) {
            if (_rw_order->request_type[i] == WRITE_REQUEST)
                insert_readwrite_order(execution_id, "WRITE", _rw_order->time[i], _rw_order->node[i], variant, statistic_file);
            else
                insert_readwrite_order(execution_id, "READ", _rw_order->time[i], _rw_order->node[i], variant, statistic_file);
        }
    }
}

/*these functions traverses an r-tree*/
static void process_snapshot_rtree(RTree *r, int execution_id, uint8_t variant, const char *statistic_file);
static void recursive_traversal_rtree(RTree *rtree, int height, int execution_id, int p_node, uint8_t variant, const char *statistic_file);
/*these functions traverses a Hilbert R-tree */
static void process_snapshot_hilbertrtree(HilbertRTree *hrtree, int execution_id, uint8_t variant, const char *statistic_file);
static void recursive_traversal_hilbertrtree(HilbertRTree *hrtree, int height, int execution_id, int p_node, uint8_t variant, const char *statistic_file);
/*these functions traverses an for-tree*/
static void process_snapshot_fortree(FORTree *fr, int execution_id, uint8_t variant, const char *statistic_file);
static void recursive_traversal_fortree(FORTree *fr, int height, int node_page, int execution_id, int p_node, uint8_t variant, const char *statistic_file);

void recursive_traversal_rtree(RTree* rtree, int height, int execution_id, int p_node, uint8_t variant, const char *statistic_file) {
    RNode *node = NULL;
    int i;
    int p;
    BBox *bbox;

    node = rnode_clone(rtree->current_node);

    //_DEBUGF(NOTICE, "Height %d", height);
    _nodes_per_level->array[height]++;

    /*internal node*/
    if (height != 0) {

        _entries_int_nodes += rtree->current_node->nofentries;
        _internal_nodes_num++;

        for (i = 0; i < rtree->current_node->nofentries; i++) {

            //we only insert if it is required
            if (variant & SO_PRINTINDEX) {
                insert_printindex(execution_id,
                        rtree->current_node->entries[i]->pointer,
                        rtree->current_node->entries[i]->bbox, i, 0, height, 0,
                        p_node, variant, statistic_file);
            }

            p = rtree->current_node->entries[i]->pointer;

            if (rtree->type == CONVENTIONAL_RTREE)
                rtree->current_node = get_rnode(&rtree->base,
                    rtree->current_node->entries[i]->pointer, height - 1);
            else if (rtree->type == FAST_RTREE_TYPE) {
                rtree->current_node = (RNode *) fb_retrieve_node(&rtree->base,
                        rtree->current_node->entries[i]->pointer, height - 1);
            } else if (rtree->type == eFIND_RTREE_TYPE) {
                rtree->current_node = (RNode *) efind_buf_retrieve_node(&rtree->base, efind_spec,
                        rtree->current_node->entries[i]->pointer, height - 1);
            } else { //it should not entered
                _DEBUGF(ERROR, "Invalid R-tree specification %d", rtree->type);
            }

            insert_arraynode(_entries_per_node,
                    create_nodeinfo(height - 1, p,
                    -1.0, rtree->current_node->nofentries));

            bbox = rnode_compute_bbox(rtree->current_node);
            insert_arraynode(_area_per_node,
                    create_nodeinfo(height - 1, p,
                    bbox_area(bbox), -1));
            lwfree(bbox);

            insert_arraynode(_ovp_area_per_node,
                    create_nodeinfo(height - 1, p,
                    rnode_overlapping_area(rtree->current_node), -1));

            insert_arraynode(_dead_space_per_node,
                    create_nodeinfo(height - 1, p,
                    rnode_dead_space_area(rtree->current_node), -1));


            recursive_traversal_rtree(rtree, height - 1, execution_id, p, 
                    variant, statistic_file);

            /*after to traverse this child, we need to back 
             * the reference of the current_node for the original one */
            rnode_copy(rtree->current_node, node);
        }
    }/* leaf node */
    else {
        _entries_leaf_nodes += rtree->current_node->nofentries;
        _leafs_nodes_num++;

        //we only insert if it is required
        if (variant & SO_PRINTINDEX) {
            for (i = 0; i < rtree->current_node->nofentries; i++) {
                insert_printindex(execution_id,
                        rtree->current_node->entries[i]->pointer,
                        rtree->current_node->entries[i]->bbox, i, 0, height, 0,
                        p_node, variant, statistic_file);
            }
        }
    }
    rnode_free(node);
    node = NULL;
}

void process_snapshot_rtree(RTree *r, int execution_id, uint8_t variant, const char *statistic_file) {
    BBox *bbox;
    int i;

    //    _DEBUG(NOTICE, "Ok");

    bbox = rnode_compute_bbox(r->current_node);
    //we only insert if it is required
    if (variant & SO_PRINTINDEX) {
        insert_printindex(execution_id, r->info->root_page, bbox, 0, 0, r->info->height, 0, -1, variant, statistic_file);
    }

    _nodes_per_level = (DynamicArrayInt*) lwalloc(sizeof (DynamicArrayInt));
    _nodes_per_level->maxelements = r->info->height + 2;
    _nodes_per_level->nofelements = r->info->height + 1;
    _nodes_per_level->array = (int*) lwalloc(sizeof (int) * _nodes_per_level->nofelements);

    //    _DEBUG(NOTICE, "instancing nodes_per_level");

    for (i = 0; i <= r->info->height; i++) {
        _nodes_per_level->array[i] = 0;
    }

    _entries_per_node = create_arraynode();
    insert_arraynode(_entries_per_node,
            create_nodeinfo(r->info->height,
            r->info->root_page, -1.0,
            r->current_node->nofentries));

    //   _DEBUG(NOTICE, "instancing entries_per_node");

    _area_per_node = create_arraynode();
    insert_arraynode(_area_per_node,
            create_nodeinfo(r->info->height,
            r->info->root_page, bbox_area(bbox), -1));
    lwfree(bbox);

    //   _DEBUG(NOTICE, "instancing area_per_node");

    _ovp_area_per_node = create_arraynode();
    insert_arraynode(_ovp_area_per_node,
            create_nodeinfo(r->info->height,
            r->info->root_page, rnode_overlapping_area(r->current_node), -1));

    //   _DEBUG(NOTICE, "instancing ovp_per_node");   

    _dead_space_per_node = create_arraynode();
    insert_arraynode(_dead_space_per_node,
            create_nodeinfo(r->info->height,
            r->info->root_page, rnode_dead_space_area(r->current_node), -1));

    //    _DEBUG(NOTICE, "instancing dead_space_per_node");

    _height = r->info->height;

    recursive_traversal_rtree(r, r->info->height, execution_id, r->info->root_page, variant, statistic_file);
}

void recursive_traversal_hilbertrtree(HilbertRTree* hrtree, int height, int execution_id, int p_node, uint8_t variant, const char *statistic_file) {
    HilbertRNode *node = NULL;
    int i;
    int p;
    BBox *bbox;
    
    //_DEBUG(NOTICE, "Traversing the node....");
    //hilbertnode_print(hrtree->current_node, p_node);

    node = hilbertnode_clone(hrtree->current_node);

    _nodes_per_level->array[height]++;

    /*internal node*/
    if (height != 0) {

        _entries_int_nodes += hrtree->current_node->nofentries;
        _internal_nodes_num++;

        for (i = 0; i < hrtree->current_node->nofentries; i++) {

            //we only insert if it is required
            if (variant & SO_PRINTINDEX) {
                insert_printindex(execution_id,
                        hrtree->current_node->entries.internal[i]->pointer,
                        hrtree->current_node->entries.internal[i]->bbox, i, 0, height,
                        hrtree->current_node->entries.internal[i]->lhv,
                        p_node, variant, statistic_file);
            }

            p = hrtree->current_node->entries.internal[i]->pointer;
            
            if (hrtree->type == CONVENTIONAL_HILBERT_RTREE)
                hrtree->current_node = get_hilbertnode(&hrtree->base,
                    hrtree->current_node->entries.internal[i]->pointer, height - 1);
            else if (hrtree->type == FAST_HILBERT_RTREE_TYPE) {
                hrtree->current_node = (HilbertRNode *) fb_retrieve_node(&hrtree->base,
                        hrtree->current_node->entries.internal[i]->pointer, height - 1);
            } else if (hrtree->type == eFIND_HILBERT_RTREE_TYPE) {
                hrtree->current_node = (HilbertRNode *) efind_buf_retrieve_node(&hrtree->base, efind_spec,
                        hrtree->current_node->entries.internal[i]->pointer, height - 1);
            } else { //it should not entered
                _DEBUGF(ERROR, "Invalid Hilbert R-tree specification %d", hrtree->type);
            }
            
            insert_arraynode(_entries_per_node,
                    create_nodeinfo(height - 1, p,
                    -1.0, hrtree->current_node->nofentries));
            
            bbox = bbox_create();
            hilbertnode_compute_bbox(hrtree->current_node, hrtree->spec->srid, bbox);
            insert_arraynode(_area_per_node,
                    create_nodeinfo(height - 1, p,
                    bbox_area(bbox), -1));
            lwfree(bbox);
            //_DEBUG(NOTICE, "collected the area");

            insert_arraynode(_ovp_area_per_node,
                    create_nodeinfo(height - 1, p,
                    hilbertnode_overlapping_area(hrtree->current_node), -1));
            //_DEBUG(NOTICE, "collected the overlapping area");

            insert_arraynode(_dead_space_per_node,
                    create_nodeinfo(height - 1, p,
                    hilbertnode_dead_space_area(hrtree->current_node, hrtree->spec->srid), -1));
            //_DEBUG(NOTICE, "collected the dead space area");

            recursive_traversal_hilbertrtree(hrtree, height - 1, execution_id, p, 
                    variant, statistic_file);

            /*after to traverse this child, we need to back 
             * the reference of the current_node for the original one */
            hilbertnode_copy(hrtree->current_node, node);
        }
    }/* leaf node */
    else {
        _entries_leaf_nodes += hrtree->current_node->nofentries;
        _leafs_nodes_num++;

        //we only insert if it is required
        if (variant & SO_PRINTINDEX) {
            for (i = 0; i < hrtree->current_node->nofentries; i++) {
                insert_printindex(execution_id,
                        hrtree->current_node->entries.leaf[i]->pointer,
                        hrtree->current_node->entries.leaf[i]->bbox, i, 0, height,
                        hilbertvalue_compute(hrtree->current_node->entries.leaf[i]->bbox, hrtree->spec->srid),
                        p_node, variant, statistic_file);
            }
        }
    }
    hilbertnode_free(node);
    node = NULL;
}

void process_snapshot_hilbertrtree(HilbertRTree *hrtree, int execution_id, uint8_t variant, const char *statistic_file) {
    BBox *bbox = bbox_create();
    hilbert_value_t hv;
    int i;

    //_DEBUG(NOTICE, "Ok");

    hv = hilbertnode_compute_bbox(hrtree->current_node, hrtree->spec->srid, bbox);
    //we only insert if it is required
    if (variant & SO_PRINTINDEX) {
        insert_printindex(execution_id, hrtree->info->root_page, bbox, 0, 0, hrtree->info->height, hv, -1, variant, statistic_file);
    }

    _nodes_per_level = (DynamicArrayInt*) lwalloc(sizeof (DynamicArrayInt));
    _nodes_per_level->maxelements = hrtree->info->height + 2;
    _nodes_per_level->nofelements = hrtree->info->height + 1;
    _nodes_per_level->array = (int*) lwalloc(sizeof (int) * _nodes_per_level->nofelements);

    //_DEBUG(NOTICE, "instancing nodes_per_level");

    for (i = 0; i <= hrtree->info->height; i++) {
        _nodes_per_level->array[i] = 0;
    }

    _entries_per_node = create_arraynode();
    insert_arraynode(_entries_per_node,
            create_nodeinfo(hrtree->info->height,
            hrtree->info->root_page, -1.0,
            hrtree->current_node->nofentries));

    //_DEBUG(NOTICE, "instancing entries_per_node");

    _area_per_node = create_arraynode();
    insert_arraynode(_area_per_node,
            create_nodeinfo(hrtree->info->height,
            hrtree->info->root_page, bbox_area(bbox), -1));
    lwfree(bbox);

    //_DEBUG(NOTICE, "instancing area_per_node");

    _ovp_area_per_node = create_arraynode();
    insert_arraynode(_ovp_area_per_node,
            create_nodeinfo(hrtree->info->height,
            hrtree->info->root_page, hilbertnode_overlapping_area(hrtree->current_node), -1));

    //_DEBUG(NOTICE, "instancing ovp_per_node");   

    _dead_space_per_node = create_arraynode();
    insert_arraynode(_dead_space_per_node,
            create_nodeinfo(hrtree->info->height,
            hrtree->info->root_page, hilbertnode_dead_space_area(hrtree->current_node, hrtree->spec->srid), -1));

    //_DEBUG(NOTICE, "instancing dead_space_per_node");

    _height = hrtree->info->height;

    recursive_traversal_hilbertrtree(hrtree, hrtree->info->height, execution_id, hrtree->info->root_page, variant, statistic_file);
    
    //_DEBUG(NOTICE, "hilbert r-tree traversed");
}

void process_snapshot_fortree(FORTree *fr, int execution_id, uint8_t variant, const char *statistic_file) {
    BBox *bbox;
    int k, j, i;
    int np;

    k = 1;
    k += fortree_get_nof_onodes(fr->info->root_page);

    //  _DEBUG(NOTICE, "Ok");

    _nodes_per_level = (DynamicArrayInt*) lwalloc(sizeof (DynamicArrayInt));
    _nodes_per_level->maxelements = fr->info->height + 2;
    _nodes_per_level->nofelements = fr->info->height + 1;
    _nodes_per_level->array = (int*) lwalloc(sizeof (int) * _nodes_per_level->nofelements);

    for (i = 0; i <= fr->info->height; i++) {
        _nodes_per_level->array[i] = 0;
    }

    _entries_per_node = create_arraynode();
    _area_per_node = create_arraynode();
    _ovp_area_per_node = create_arraynode();
    _dead_space_per_node = create_arraynode();

    rnode_free(fr->current_node);

    for (j = 0; j < k; j++) {
        if (j == 0) {
            fr->current_node = forb_retrieve_rnode(&fr->base, fr->info->root_page, fr->info->height);
            np = fr->info->root_page;
        } else {
            rnode_free(fr->current_node);
            np = fortree_get_onode(fr->info->root_page, j - 1);
            fr->current_node = forb_retrieve_rnode(&fr->base, np, fr->info->height);
        }
        bbox = rnode_compute_bbox(fr->current_node);
        //we only insert if it is required
        if (variant & SO_PRINTINDEX) {
            if (j > 0)
                insert_printindex(execution_id, np, bbox, j, 1, fr->info->height, 0, -1, variant, statistic_file);
            else
                insert_printindex(execution_id, np, bbox, j, 0, fr->info->height, 0, -1, variant, statistic_file);
        }

        insert_arraynode(_entries_per_node,
                create_nodeinfo(fr->info->height,
                np, -1.0,
                fr->current_node->nofentries));

        insert_arraynode(_area_per_node,
                create_nodeinfo(fr->info->height,
                np, bbox_area(bbox), -1));
        lwfree(bbox);

        insert_arraynode(_ovp_area_per_node,
                create_nodeinfo(fr->info->height,
                np, rnode_overlapping_area(fr->current_node), -1));

        insert_arraynode(_dead_space_per_node,
                create_nodeinfo(fr->info->height,
                np, rnode_dead_space_area(fr->current_node), -1));
    }
    rnode_free(fr->current_node);
    fr->current_node = forb_retrieve_rnode(&fr->base, fr->info->root_page, fr->info->height);

    _height = fr->info->height;

    recursive_traversal_fortree(fr, fr->info->height, fr->info->root_page, execution_id, fr->info->root_page, variant, statistic_file);
}

void recursive_traversal_fortree(FORTree *fr, int height, int node_page, int execution_id, int p_node, uint8_t variant, const char *statistic_file) {
    RNode *node;
    int node_p;
    int i, k, j;
    BBox *bbox;
    int parent;

    node = rnode_clone(fr->current_node);

    k = 1;
    k += fortree_get_nof_onodes(node_page);

    _nodes_per_level->array[height]++;

    if (height != 0) {

        _entries_int_nodes += fr->current_node->nofentries;
        _internal_nodes_num++;

        for (j = 0; j < k; j++) {
            if (j > 0) {
                parent = fortree_get_onode(node_page, j - 1);
                rnode_free(fr->current_node);
                fr->current_node = forb_retrieve_rnode(&fr->base, parent, height);

                rnode_copy(node, fr->current_node);

                _entries_int_o_nodes += fr->current_node->nofentries;
                _int_o_nodes_num++;
            } else {
                parent = p_node;
            }

            for (i = 0; i < fr->current_node->nofentries; i++) {
                //we only insert if it is required
                if (variant & SO_PRINTINDEX) {
                    if (j > 0) {
                        insert_printindex(execution_id,
                                fr->current_node->entries[i]->pointer,
                                fr->current_node->entries[i]->bbox, i, 1, height, 0,
                                parent, variant, statistic_file);
                    } else {
                        insert_printindex(execution_id,
                                fr->current_node->entries[i]->pointer,
                                fr->current_node->entries[i]->bbox, i, 0, height, 0,
                                parent, variant, statistic_file);
                    }
                }

                node_p = fr->current_node->entries[i]->pointer;
                fr->current_node = forb_retrieve_rnode(&fr->base, node_p, height - 1);

                insert_arraynode(_entries_per_node,
                        create_nodeinfo(height - 1, node_p,
                        -1.0, fr->current_node->nofentries));

                bbox = rnode_compute_bbox(fr->current_node);
                insert_arraynode(_area_per_node,
                        create_nodeinfo(height - 1, node_p,
                        bbox_area(bbox), -1));
                lwfree(bbox);

                insert_arraynode(_ovp_area_per_node,
                        create_nodeinfo(height - 1, node_p,
                        rnode_overlapping_area(fr->current_node), -1));

                insert_arraynode(_dead_space_per_node,
                        create_nodeinfo(height - 1, node_p,
                        rnode_dead_space_area(fr->current_node), -1));

                recursive_traversal_fortree(fr, height - 1, node_p, execution_id, 
                        parent, variant, statistic_file);

                /*after to traverse this child, we need to back 
                 * the reference of the current_node for the original one */
                rnode_copy(fr->current_node, node);
            }
        }
    } else {

        _entries_leaf_nodes += fr->current_node->nofentries;
        _leafs_nodes_num++;

        for (j = 0; j < k; j++) {
            if (j > 0) {
                parent = fortree_get_onode(node_page, j - 1);
                rnode_free(fr->current_node);
                fr->current_node = forb_retrieve_rnode(&fr->base, parent, height);

                rnode_copy(node, fr->current_node);

                _entries_leaf_o_nodes += fr->current_node->nofentries;
                _leaf_o_nodes_num++;

            } else {
                parent = p_node;
            }
            for (i = 0; i < fr->current_node->nofentries; i++) {
                //we only insert if it is required
                if (variant & SO_PRINTINDEX) {
                    if (j > 0) {
                        insert_printindex(execution_id,
                                fr->current_node->entries[i]->pointer,
                                fr->current_node->entries[i]->bbox, i, 1, height, 0,
                                parent, variant, statistic_file);
                    } else {
                        insert_printindex(execution_id,
                                fr->current_node->entries[i]->pointer,
                                fr->current_node->entries[i]->bbox, i, 0, height, 0,
                                parent, variant, statistic_file);
                    }
                }
            }
        }
    }
    rnode_free(node);
}

/*this function inserts the data into tables in the PostgreSQL*/
int process_statistic_information(SpatialIndex *si, uint8_t variant, const char *statistic_file) {
    /*
     * Here, we assume that src_id, bc_id, sc_id, and buf_id are already stored in the FESTIval's data schema!
     * these values are already stored in their respective structs
     */
    int execution_id = -1;
    int idx_id = -1;

    //_DEBUG(NOTICE, "Processing statistical info");

    if (variant & SO_STORE_STATISTICAL_IN_FILE) {
        //we will store the statistical data in a SQL file, which can be imported to PostgreSQL later        
        FILE *file;
        //we open the file
        file = fopen(statistic_file, "a+");
        if (file == NULL) {
            _DEBUGF(ERROR, "The file %s cannot be opened.", statistic_file);
        } else {
            //we create the temporary table that will be used to store the execution_id
            fprintf(file, "CREATE TEMP TABLE IF NOT EXISTS execution_id_temp "
                    "(id INTEGER) "
                    "ON COMMIT DELETE ROWS;\n");
            //ON COMMIT DELETE ROWS specifies that the data are removed from the temporary table at the end of each transaction

            //we then start a new transaction:
            fprintf(file, "BEGIN TRANSACTION;\n");
        }
        //we close the SQL file
        fclose(file);
    } else {
        int config_id;
        //we will store the statistical data directly in the FESTIval's data schema
        //insert index_configuration
        config_id = insert_statistic_indexconfig(si);

        //insert spatial index
        idx_id = insert_statistic_spatialindex(si, config_id);
    }

    if (variant & SO_EXECUTION) {
        //insert the execution
        execution_id = insert_execution(si, idx_id, variant, statistic_file);
    }

    //we have to store the flash simulation results if a flash simulator is being used and it was set by the user
    if (variant & SO_FLASHSIMULATOR) {
        if (si->gp->storage_system->type != FLASHDBSIM) {
            _DEBUGF(WARNING, "FESTIval cannot collect flash simulation results from this storage system id: %d",
                    si->gp->storage_system->ss_id);
        } else {
            insert_flashsimulator_statistics(execution_id, variant, statistic_file);
        }
    }

    //_DEBUG(NOTICE, "Processed");

    //if the corresponding flag is on
    if (variant & (SO_INDEXSNAPSHOT | SO_PRINTINDEX)) {
        //_DEBUG(NOTICE, "processing snapshot");
        process_index_snapshot(si, execution_id, variant, statistic_file);
        //_DEBUG(NOTICE, "done");
    }

    //we have to store the order of read and write operations
    if (_COLLECT_READ_WRITE_ORDER == 1) {
        process_readwrite_order(execution_id, variant, statistic_file);
    }

    if (variant & SO_STORE_STATISTICAL_IN_FILE) {
        FILE *file;
        //we open the file
        file = fopen(statistic_file, "a+");
        if (file == NULL) {
            _DEBUGF(ERROR, "The file %s cannot be opened.", statistic_file);
        } else {
            //we then commit the transaction related to the storage of this statistical data
            fprintf(file, "COMMIT;\n\n");
        }
        //we close the SQL file
        fclose(file);
    }

    return execution_id;
}

/* we need only a snapshot when there are modifications
         for instance: creation, inserts, updates, and so on
 */
void process_index_snapshot(SpatialIndex *si, int execution_id, uint8_t variant, const char *statistic_file) {
    uint8_t index_type = spatialindex_get_type(si);
    //insert the snapshot of the execution (how the index looks like after this execution?)
    if (index_type == CONVENTIONAL_RTREE) {
        RTree *rtree = (void *) si;
        //it also inserts the printindex if its flag is on
        process_snapshot_rtree(rtree, execution_id, variant, statistic_file);
    } else if (index_type == CONVENTIONAL_RSTARTREE) {
        RStarTree *rstar = (void *) si;
        RTree *rtree;

        //we first convert the rstartree to an rtree since this is the same search algorithm
        rtree = rstartree_to_rtree(rstar);

        //it also inserts the printindex if its flag is on
        process_snapshot_rtree(rtree, execution_id, variant, statistic_file);
        free_converted_rtree(rtree);
    } else if(index_type == CONVENTIONAL_HILBERT_RTREE) {
        HilbertRTree *hrtree = (void*) si;
        process_snapshot_hilbertrtree(hrtree, execution_id, variant, statistic_file);
    } else if (index_type == FAST_RTREE_TYPE) {
        FASTIndex *fi = (void *) si;
        FASTRTree *fr;
        fr = fi->fast_index.fast_rtree;
        rtree_set_fastspecification(fr->spec);
        //it also inserts the printindex if its flag is on
        process_snapshot_rtree(fr->rtree, execution_id, variant, statistic_file);
    } else if (index_type == FAST_RSTARTREE_TYPE) {
        FASTIndex *fi = (void *) si;
        RTree *r;
        FASTRStarTree *fr;
        RStarTree *rstar;

        fr = fi->fast_index.fast_rstartree;
        rstar = fr->rstartree;
        /* a critical problem:
         if a insertion or remotion is done after its creation (only in the same session)
         */
        r = rstartree_to_rtree(rstar);

        //it also inserts the printindex if its flag is on
        process_snapshot_rtree(r, execution_id, variant, statistic_file);
        free_converted_rtree(r);
    } else if(index_type == FAST_HILBERT_RTREE_TYPE) {
        FASTIndex *fi = (void *) si;
        FASTHilbertRTree *fr;
        fr = fi->fast_index.fast_hilbertrtree;
        hilbertrtree_set_fastspecification(fr->spec);
        //it also inserts the printindex if its flag is on
        process_snapshot_hilbertrtree(fr->hilbertrtree, execution_id, variant, statistic_file);
    } else if (index_type == FORTREE_TYPE) {
        FORTree *fr = (void *) si;
        //it also inserts the printindex if its flag is on
        process_snapshot_fortree(fr, execution_id, variant, statistic_file);
    } else if (index_type == eFIND_RTREE_TYPE) {
        eFINDIndex *fi = (void *) si;
        eFINDRTree *fr;
        fr = fi->efind_index.efind_rtree;
        rtree_set_efindspecification(fr->spec);
        efind_spec = fr->spec;
               
        //it also inserts the printindex if its flag is on
        process_snapshot_rtree(fr->rtree, execution_id, variant, statistic_file);
        efind_spec = NULL;
    } else if (index_type == eFIND_RSTARTREE_TYPE) {
        eFINDIndex *fi = (void *) si;
        RTree *r;
        eFINDRStarTree *fr;
        RStarTree *rstar;

        fr = fi->efind_index.efind_rstartree;
        rstar = fr->rstartree;
        r = rstartree_to_rtree(rstar);
        efind_spec = fr->spec;

        //it also inserts the printindex if its flag is on
        process_snapshot_rtree(r, execution_id, variant, statistic_file);
        free_converted_rtree(r);
        efind_spec = NULL;
    } else if(index_type == eFIND_HILBERT_RTREE_TYPE) {
        eFINDIndex *fi = (void *) si;
        eFINDHilbertRTree *fr;
        fr = fi->efind_index.efind_hilbertrtree;
        hilbertrtree_set_efindspecification(fr->spec);
        efind_spec = fr->spec;
               
        //it also inserts the printindex if its flag is on
        process_snapshot_hilbertrtree(fr->hilbertrtree, execution_id, variant, statistic_file);
        efind_spec = NULL;
    }
    insert_snapshot(execution_id, variant, statistic_file);
}
