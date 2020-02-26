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
 * File:   statistical_processing.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 24, 2016, 6:26 PM
 */

#ifndef STATISTICAL_PROCESSING_H
#define STATISTICAL_PROCESSING_H

#include "spatial_index.h"
#include <sys/time.h> /* for elapsed time, obtain the current time */
#include <time.h> /* for CPU time */

extern uint8_t _STORING;
extern uint8_t _COLLECT_READ_WRITE_ORDER;

//we can define it by using -DCOLLECT_STATISTICAL_DATA in the Makefile
#define COLLECT_STATISTICAL_DATA    //this means that we have to collect the statistical data

/*structure used for manage simple array of ints*/
typedef struct {
    int nofelements;
    int maxelements;
    int *array;
} DynamicArrayInt;

/*structure to manage the order of read/write requests*/
#define WRITE_REQUEST   1
#define READ_REQUEST    2
typedef struct {
    int nofelements;
    int maxelements;
    int *node;
    uint8_t *request_type;
    double *time;
} RWOrder; 

typedef struct {
    int level;
    int id;
    double db_value; //if the value is a double then we use this variable
    int int_value; //otherwise, we use this variable
} NodeInfo;

typedef struct {
    int nofelements;
    int maxelements;
    NodeInfo **array;
} ArrayNode;

/*
** Variants for collection of statistical data
*/
#define SO_EXECUTION 0x01 /* User wants to collect statistical data for Execution table */
#define SO_INDEXSNAPSHOT 0x02 /* User wants to collect the index snapshot info */
#define SO_PRINTINDEX 0x04 /* User wants to collect the print index info */
#define SO_FLASHSIMULATOR 0x08 /* User wants to collect statistics from the flash simulator */

#define SO_STORE_STATISTICAL_IN_FILE 0x10 /*User wants to store its statistical information in a file */

extern char *_execution_name; //the execution name of the workload (done)

extern uint8_t _query_predicate; //the predicate when the operation is a query (e.g., OVERLAPS) (done)

/* variables to manage time in ms (elapsed time) */
extern double _total_time; //total time to completely process an operation (e.g., a range query) (done)
extern double _index_time; //time that the spatial index took to return the set of candidates (done)
extern double _filter_time; //time for the filter step for a spatial query with index (done)
extern double _refinement_time; //time for the refinement step for a spatial query with index (this indicates the time of 9-IM processing) (done)
extern double _retrieving_objects_time; //time for get the spatial objects from PostgreSQL (done)
extern double _processing_predicates_time; //time to process topological predicates in the refinement step (done)
extern double _read_time; //total time for all performed read operations (done)
extern double _write_time; //total time for all performed write operations (done)
extern double _split_time; //total time for all performed split operations (done)

/* variables to manage time in ms (CPU time) */
extern double _total_cpu_time; //total time to completely process an operation (e.g., a range query) (done)
extern double _index_cpu_time; //time that the spatial index took to return the set of candidates (done)
extern double _filter_cpu_time; //time for the filter step for a spatial query with index (done)
extern double _refinement_cpu_time; //time for the refinement step for a spatial query with index (this indicates the time of 9-IM processing) (done)
extern double _retrieving_objects_cpu_time; //time for get the spatial objects from PostgreSQL (done)
extern double _processing_predicates_cpu_time; //time to process topological predicates in the refinement step (done)
extern double _read_cpu_time; //total time for all performed read operations (done)
extern double _write_cpu_time; //total time for all performed write operations (done)
extern double _split_cpu_time; //total time for all performed split operations (done)

/* variables to manage numbers/amounts */
extern int _cand_num; //number of candidates returned by the filtering step (done)
extern int _result_num; //number of returned spatial objects of a query (done)
extern int _read_num; //number of read operations (done)
extern int _write_num; //number of write operations (done)
extern int _split_int_num; //number of split operations done in the internal nodes (done)
extern int _split_leaf_num; //number of split operations done in the leaf nodes (done)
extern unsigned long long int _processed_entries_num; //number of visited entries in the: search, choose_node, find_leaf algorithms, reinsert of the r*-tree
//to get the visited entries in the nodes that was split we can: _split_{int, leaf}_num * (M+1)
extern int _reinsertion_num; //number of times that the reinsertion policy was performed
extern int _visited_int_node_num; //number of visited internal nodes in an operation --this is to ACCESS a node (done)
extern int _visited_leaf_node_num; //number of visited leaf nodes in an operation --this is to ACCESS a node (done)
extern int _written_int_node_num; //number of written internal nodes in an operation --this is to WRITE a node (done)
extern int _written_leaf_node_num; //number of written leaf nodes in an operation --this is to WRITE a node (done)
extern int _deleted_int_node_num; //number of deleted internal nodes in an operation --this is to REMOVE a node (done)
extern int _deleted_leaf_node_num; //number of deleted leaf nodes in an operation --this is to REMOVE a node (done)
extern DynamicArrayInt *_writes_per_height; //number of writes (for modification or remotion) per level of the tree (this is an array) (done)
extern DynamicArrayInt *_reads_per_height; //number of reads per level of the tree (this is an array) (done)
extern RWOrder *_rw_order; //an array with the order of the read write request and the page number of this request (done)

/* variables to collect statistical information regarding to flushing operations */
extern double _flushing_time; //time to flush a part of buffer to SSD (done)
extern double _flushing_cpu_time; //the same of previous but for cpu time (done)
extern int _flushing_num; //number of performed flushing (done)
extern int _flushed_nodes_num; //the number of the total nodes that was flushing (done)

/* variables with regard to the processing in SSDs (it may vary according to the used spatial indexing)*/
//total values
extern int _mod_node_buffer_num; //TOTAL number of modified entries that passed in the buffer (done)
extern int _new_node_buffer_num; //TOTAL number of newly created nodes that was stored in the buffer (done)
extern int _del_node_buffer_num; //TOTAL number of removed nodes that was stored in the buffer (done)
//current values (actual values)
extern int _cur_mod_node_buffer_num; //number of modified entries in the buffer (done)
extern int _cur_new_node_buffer_num; //number of newly created nodes in the buffer (done)
extern int _cur_del_node_buffer_num; //number of removed nodes in the buffer (done)
extern int _cur_buffer_size; //the size in bytes of the buffer after an operation (done)

extern double _ret_node_from_buf_time; //total time that was waste to retrieve nodes from the buffer (done)
extern double _ret_node_from_buf_cpu_time; //total time that was waste to retrieve nodes from the buffer (cpu time) (done)

extern double _write_log_time; //total time to manage the writes performed on the log 
extern double _write_log_cpu_time; //total CPU time to manage the writes performed on the log
extern double _compactation_log_time; //time to compact the log (for durability) (done)
extern double _compactation_log_cpu_time; //the same of previous but for cpu time (done)
extern double _recovery_log_time; //time to recovery the buffer in main memory from log (for durability) (done)
extern double _recovery_log_cpu_time; //the same of previous but for cpu time (done)
extern int _compactation_log_num; //number of times that the compaction of log was done (done)
extern int _write_log_num; //number of writes in the log (to guarantee durability) (done)
extern int _read_log_num; //number of reads in the log (this is only computed if there was compaction) (done)
extern int _cur_log_size; //the size in bytes of the log file after an operation (done)

extern int _nof_unnecessary_flushed_nodes; //since FAST organizes flushing units in a static way,
//there is the possibility to flush nodes that do not have modifications
//this is a counter for these cases

/* variables about the constructed index, 
 * this is collected from a specific function to read the index
 * this is the "snapshot" of the index after an execution */
extern int _height; //height of the tree (done)
extern int _entries_int_nodes; //number of occupied entries in internal nodes (done)
extern int _entries_leaf_nodes; //number of occupied entries in leaf nodes (done)
extern int _internal_nodes_num; //number of the created internal nodes (done)
extern int _leafs_nodes_num; //number of the created leaf nodes (done)

/*for for-tree*/
extern int _int_o_nodes_num; //number of internal o-nodes (done)
extern int _leaf_o_nodes_num; //number of leaf o-nodes (done)
extern int _entries_int_o_nodes; //number of entries in internal o-nodes (done)
extern int _entries_leaf_o_nodes; //number of entries in leaf o-nodes (done)
extern int _merge_back_num; //number of merge-back operations (done)

/* variable for the collection of statistical data of standard buffers (e.g., LRU) */
extern int _sbuffer_page_fault;
extern int _sbuffer_page_hit;
//the time to do the following things: if a find fail, we read the node from disk
//put it into the buffer (which can lead to a flushing time, if such node has modifications)
extern double _sbuffer_find_time;
extern double _sbuffer_find_cpu_time; 
//this flushing time is different from the flushing time of flash-aware spatial indices
//since this is for the standard buffers
extern double _sbuffer_flushing_time; 
extern double _sbuffer_flushing_cpu_time; 

/*for efind indices*/
extern int _read_buffer_page_hit; //number of hits done by the read buffer of the fasinf (done)
extern int _read_buffer_page_fault; //number of faults done by the read buffer of the fasinf (done)
extern int _cur_read_buffer_size; //read buffer size of the fasinf (done)
extern double _read_buffer_put_node_cpu_time; //consuming time to put a node in the read buffer (done)
extern double _read_buffer_put_node_time; //consuming time to put a node in the read buffer (done)
extern double _read_buffer_get_node_cpu_time; //consuming time to get a node from the read buffer (done)
extern double _read_buffer_get_node_time; //consuming time to get a node from the read buffer (done)
extern int _efind_force_node_in_read_buffer; //the number of times that the temporal control for reads was performed (done)
extern int _efind_write_temporal_control_sequential; //the number of that the temporal control for writes was performed (sequential version) (done)
extern int _efind_write_temporal_control_stride; //the number of that the temporal control for writes was performed (stride version) (done)
extern int _efind_write_temporal_control_seqstride; //the number of that the temporal control for writes was performed (mixed version) (done)
extern int _efind_write_temporal_control_filled; //the number of that the temporal control for writes had to be completed with random nodes (done)

/*the other statistical values with respect to the flash simulator are extracted from the flashdbsim global variables*/

/************************************
 functions to manage the variables
 ************************************/

//this function resets/sets all the variables to zero
extern void initiate_statistic_values(void);

//this function inserts data about collectable variables into tables in the PostgreSQL
extern int process_statistic_information(SpatialIndex *si, uint8_t variant, const char *statistic_file);

/*this function only inserts statistical data related to the snapshot of the index*/
extern void process_index_snapshot(SpatialIndex *si, int execution_id, uint8_t variant, const char *statistic_file);

/*this function increment the number of writes (second parameter) in a determined level of the tree (first parameter)*/
extern void insert_writes_per_height(int height, int incremented_v);

/*this function increment the number of writes (second parameter) in a determined level of the tree (first parameter)*/
extern void insert_reads_per_height(int height, int incremented_v);

/*this function appends a read/write request of a given page number */
extern void append_rw_order(int page_num, uint8_t type, double time);

//this function free allocated memory
extern void statistic_free_allocated_memory(void);

//this function reset all global variables related to the time/count collections
extern void statistic_reset_variables(void);

/*this function returns the CPU time in order to compute the CPU time used*/
extern struct timespec get_CPU_time(void);

//this function returns the elapsed time in order to compute the total elapsed time of an operation
extern struct timespec get_current_time(void);

//this function returns the elapsed time in order to compute the total elapsed time of an operation
extern double get_current_time_in_seconds(void);

//calculate the total processed time and return the elapsed total time in SECONDS!
extern double get_elapsed_time(struct timespec start, struct timespec end);


#endif /* _STATISTICAL_PROCESSING_H */
