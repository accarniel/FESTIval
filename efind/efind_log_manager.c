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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for O_DIRECT */
#endif

#include <sys/types.h>  /* required by open() */
#include <unistd.h>     /* open(), write() */
#include <fcntl.h>      /* open() and fcntl() */
#include <string.h>
#include <stringbuffer.h> /* for memcpy */

#include "../main/io_handler.h" //for DIRECT and NORMAL access

#include "efind_buffer_manager.h" //buffer operations
#include "efind_log_manager.h" //basic functions

#include "../main/log_messages.h" //debug messages

#include "../main/statistical_processing.h"

static void raw_write_log(const char *file, uint8_t *buf, size_t bufsize);
static void raw_read_log(const char *file, size_t offset, uint8_t *buf, size_t bufsize);
/*this function process the retrieved entry of the log 
 * it push in the redostack */
static eFINDLogEntry *efind_retrieve_log_entry(uint8_t *buf, size_t *offset, uint8_t index_type);
static void efind_log_entry_free(eFINDLogEntry* le, uint8_t index_type);

static int min_nof_flushing = 1; //a  global variable to decide if a compaction log will be performed or not - it starts with the value 1
static int nof_flushing = 0; //the current number of flushings stored in the log file (that we now in the main memory)

void raw_write_log(const char* file, uint8_t *buf, size_t bufsize) {
    int f;
    size_t written;
    if ((f = open(file, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR)) < 0) {
        _DEBUGF(ERROR, "It was impossible to open the \'%s\'. ", file);
        return;
    }
    if ((written = write(f, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Written size (%zu) not equal to buffer size (%zu) in raw_write_log!", written, bufsize);
        return;
    }
    close(f);
#ifdef COLLECT_STATISTICAL_DATA
    _write_log_num++;
#endif
}

void raw_read_log(const char* file, size_t offset, uint8_t *buf, size_t bufsize) {
    int f;
    size_t read_size;
    if ((f = open(file, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) < 0) {
        _DEBUGF(ERROR, "It was impossible to open the \'%s\'. ", file);
        return;
    }
    if (lseek(f, offset, SEEK_SET) < 0) {
        _DEBUGF(ERROR, "Error in lseek in raw_read_log for the file \'%s\'", file);
    }
    if ((read_size = read(f, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Read size (%zu) not equal to buffer size (%zu) in raw_read_log!", read_size, bufsize);
        return;
    }
    close(f);
#ifdef COLLECT_STATISTICAL_DATA
    _read_log_num++;
#endif
}

void efind_log_entry_free(eFINDLogEntry* le, uint8_t index_type) {
    if (le->status == eFIND_STATUS_MOD) {
        if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
            if (le->value.mod->entry != NULL) {
                REntry *entry = (REntry *) le->value.mod->entry;
                lwfree(entry->bbox);
                lwfree(entry);
            }
        } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
            if (le->value.mod->entry != NULL) {
                if (le->height > 0) {
                    HilbertIEntry *entry = (HilbertIEntry *) le->value.mod->entry;
                    lwfree(entry->bbox);
                    lwfree(entry);
                } else {
                    REntry *entry = (REntry *) le->value.mod->entry;
                    lwfree(entry->bbox);
                    lwfree(entry);
                }
            }
        } else {
            _DEBUGF(ERROR, "eFIND does not support this index (%d) yet.", index_type);
        }
        lwfree(le->value.mod);
    } else if (le->status == eFIND_STATUS_FLUSH) {
        if (le->value.flushed_nodes->pages_id != NULL)
            lwfree(le->value.flushed_nodes->pages_id);
        lwfree(le->value.flushed_nodes);
    } else {
        if (le->status != eFIND_STATUS_NEW && le->status != eFIND_STATUS_DEL)
            _DEBUGF(ERROR, "Unknown status in the log file: %d", le->status);
    }
    lwfree(le);
}

static size_t efind_size_of_create_node(void);
static size_t efind_size_of_mod_node(void *entry, int height, uint8_t index_type);
static size_t efind_size_of_del_node(void);
static size_t efind_size_of_flushed_nodes(int n);

size_t efind_size_of_create_node() {
    size_t bufsize;
    //the size of previous element in the log file and the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    //size of the page_id
    bufsize += sizeof (int);
    //size of the height
    bufsize += sizeof (uint32_t);

    return bufsize;
}

size_t efind_size_of_mod_node(void *entry, int height, uint8_t index_type) {
    size_t bufsize;
    //the size of previous element in the log file, the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    //flag (to indicate if entry is null or not) and the identifier of the page
    bufsize += sizeof (uint8_t) + sizeof (int);
    //size of the height
    bufsize += sizeof (uint32_t);
    if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
        REntry *re = (REntry*) entry;
        //size of pointer + bbox
        if (re->bbox != NULL)
            bufsize += sizeof (int) + sizeof (BBox);
        else
            bufsize += sizeof (int);
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
        if (height > 0) {
            HilbertIEntry *re = (HilbertIEntry*) entry;
            //size of pointer + bbox
            if (re->bbox != NULL)
                bufsize += sizeof (int) + sizeof (BBox) + sizeof (hilbert_value_t);
            else
                bufsize += sizeof (int);
        } else {
            REntry *re = (REntry*) entry;
            //size of pointer + bbox
            if (re->bbox != NULL)
                bufsize += sizeof (int) + sizeof (BBox);
            else
                bufsize += sizeof (int);
        }
    } else {
        _DEBUGF(ERROR, "eFIND does not support this index (%d) yet.", index_type);
    }


    return bufsize;
}

size_t efind_size_of_del_node() {
    size_t bufsize;
    //the size of previous element in the log file and the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    //size of the page_id
    bufsize += sizeof (int);
    //size of the height
    bufsize += sizeof (uint32_t);
    return bufsize;
}

size_t efind_size_of_flushed_nodes(int n) {
    size_t bufsize;
    //the size of previous element in the log file and the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    //size of the flushed_nodes
    bufsize += sizeof (int) + (sizeof (int) * n);
    return bufsize;
}

eFINDLogEntry *efind_retrieve_log_entry(uint8_t *buf, size_t *offset, uint8_t index_type) {
    eFINDLogEntry *ret;
    size_t p;

    if (buf == NULL) {
        _DEBUG(ERROR, "Buffer is null in retrieve_log_entry");
        return NULL;
    }

    ret = (eFINDLogEntry*) lwalloc(sizeof (eFINDLogEntry));

    memcpy(&p, buf, sizeof (size_t));
    buf += sizeof (size_t);

    memcpy(&(ret->status), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    if (ret->status == eFIND_STATUS_NEW) {
        memcpy(&(ret->page_id), buf, sizeof (int));
        buf += sizeof (int);

        memcpy(&(ret->height), buf, sizeof (uint32_t));
        buf += sizeof (uint32_t);
    } else if (ret->status == eFIND_STATUS_MOD) {
        uint8_t flag;

        memcpy(&(ret->page_id), buf, sizeof (int));
        buf += sizeof (int);

        memcpy(&(ret->height), buf, sizeof (uint32_t));
        buf += sizeof (uint32_t);

        ret->value.mod = (eFINDLogValue*) lwalloc(sizeof (eFINDLogValue));

        memcpy(&flag, buf, sizeof (uint8_t));
        buf += sizeof (uint8_t);

        if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
            BBox *bbox;
            int p;
            //we read the pointer of the entry
            memcpy(&p, buf, sizeof (int));
            buf += sizeof (int);

            if (flag == 0) {
                bbox = NULL;
            } else {
                //we read the bbox
                bbox = (BBox*) lwalloc(sizeof (BBox));
                memcpy(bbox, buf, sizeof (BBox));
                buf += sizeof (BBox);
            }

            ret->value.mod->entry = (void *) rentry_create(p, bbox);
        } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {
            BBox *bbox;
            int p;
            hilbert_value_t hv = 0;
            //we read the pointer of the entry
            memcpy(&p, buf, sizeof (int));
            buf += sizeof (int);

            if (flag == 0) {
                bbox = NULL;
            } else {
                if (ret->height > 0) {
                    //we read the lhv of the entry
                    memcpy(&hv, buf, sizeof (hilbert_value_t));
                    buf += sizeof (hilbert_value_t);
                }

                //we read the bbox
                bbox = (BBox*) lwalloc(sizeof (BBox));
                memcpy(bbox, buf, sizeof (BBox));
                buf += sizeof (BBox);
            }

            if (ret->height == 0) {
                ret->value.mod->entry = (void *) rentry_create(p, bbox);
            } else {
                ret->value.mod->entry = (void *) hilbertentry_create(p, bbox, hv);
            }

        } else {
            _DEBUGF(ERROR, "eFIND does not support this index (%d) yet.", index_type);
        }
    } else if (ret->status == eFIND_STATUS_DEL) {
        memcpy(&(ret->page_id), buf, sizeof (int));
        buf += sizeof (int);

        memcpy(&(ret->height), buf, sizeof (uint32_t));
        buf += sizeof (uint32_t);
    } else if (ret->status == eFIND_STATUS_FLUSH) {
        int n;
        ret->value.flushed_nodes = (eFINDFlushedNodes*) lwalloc(sizeof (eFINDFlushedNodes));

        memcpy(&n, buf, sizeof (int));
        buf += sizeof (int);

        ret->value.flushed_nodes->n = n;
        if (n <= 0) {
            _DEBUG(WARNING, "There is no flushing nodes in the log...");
            ret->value.flushed_nodes->pages_id = NULL;
        } else {
            ret->value.flushed_nodes->pages_id = (int*) lwalloc(sizeof (int)*n);
            memcpy(ret->value.flushed_nodes->pages_id, buf, sizeof (int)*n);
            buf += (sizeof (int)*n);
        }
    } else {
        _DEBUGF(ERROR, "Unknown status in the log file: %d", ret->status);
        return NULL;
    }

    if (offset)
        *offset = p;

    return ret;
}

void efind_write_log_create_node(const SpatialIndex *base, eFINDSpecification *spec,
        int new_node_page, int height) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t t = eFIND_STATUS_NEW;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
#endif 
    //if log_size <= 0 then we do not have any control of data durability
    if (spec->log_size <= 0)
        return;

#ifdef COLLECT_STATISTICAL_DATA  
    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    bufsize = efind_size_of_create_node();

    //if we our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        efind_compact_log(base, spec);
    }

    if (base->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, base->gp->page_size, bufsize)) {
            _DEBUG(ERROR, "Allocation failed at execute_flushing");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(bufsize);
    }

    loc = buf;

    //now we have to serialize the offset of the previous element
    memcpy(loc, &(spec->offset_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    //now we serialize the type of modification
    memcpy(loc, &t, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //now we serialize the identifier of the page
    memcpy(loc, &new_node_page, sizeof (int));
    loc += sizeof (int);

    //now we serialize the height
    memcpy(loc, &height, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    if (base->gp->io_access == DIRECT_ACCESS)
        free(buf);
    else
        lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

void efind_write_log_mod_node(const SpatialIndex *base, eFINDSpecification *spec,
        int node_page, void *entry, int height) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t t = eFIND_STATUS_MOD;
    uint8_t flag;
    uint8_t index_type;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
#endif 

    //if log_size <= 0 then we do not have any control of data durability
    if (spec->log_size <= 0)
        return;

#ifdef COLLECT_STATISTICAL_DATA  
    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    index_type = spatialindex_get_type(base);
    bufsize = efind_size_of_mod_node(entry, height, index_type);

    //if we our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        efind_compact_log(base, spec);
    }

    if (base->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, base->gp->page_size, bufsize)) {
            _DEBUG(ERROR, "Allocation failed at execute_flushing");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(bufsize);
    }

    loc = buf;

    //now we have to serialize the offset of the previous element
    memcpy(loc, &(spec->offset_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    //now we serialize the type of modification
    memcpy(loc, &t, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //now we serialize the identifier of the page
    memcpy(loc, &node_page, sizeof (int));
    loc += sizeof (int);

    //now we serialize the height
    memcpy(loc, &height, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    //now we serialize the entry
    if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE) {
        REntry *re = (REntry*) entry;

        if (re->bbox == NULL) {
            flag = 0;
            memcpy(loc, &flag, sizeof (uint8_t));
            loc += sizeof (uint8_t);

            //we write the pointer of the entry
            memcpy(loc, &(re->pointer), sizeof (int));
            loc += sizeof (int);
        } else {
            flag = 1;
            memcpy(loc, &flag, sizeof (uint8_t));
            loc += sizeof (uint8_t);

            //we write the pointer of the entry
            memcpy(loc, &(re->pointer), sizeof (int));
            loc += sizeof (int);

            //we write the bbox
            memcpy(loc, re->bbox, sizeof (BBox));
            loc += sizeof (BBox);
        }
    } else if (index_type == eFIND_HILBERT_RTREE_TYPE) {

        if (height > 0) {
            HilbertIEntry *re = (HilbertIEntry*) entry;

            if (re->bbox == NULL) {
                flag = 0;
                memcpy(loc, &flag, sizeof (uint8_t));
                loc += sizeof (uint8_t);

                //we write the pointer of the entry
                memcpy(loc, &(re->pointer), sizeof (int));
                loc += sizeof (int);
            } else {
                flag = 1;
                memcpy(loc, &flag, sizeof (uint8_t));
                loc += sizeof (uint8_t);

                //we write the pointer of the entry
                memcpy(loc, &(re->pointer), sizeof (int));
                loc += sizeof (int);

                //we write the lhv of the entry
                memcpy(loc, &(re->lhv), sizeof (hilbert_value_t));
                loc += sizeof (hilbert_value_t);

                //we write the bbox
                memcpy(loc, re->bbox, sizeof (BBox));
                loc += sizeof (BBox);
            }
        } else {
            REntry *re = (REntry*) entry;

            if (re->bbox == NULL) {
                flag = 0;
                memcpy(loc, &flag, sizeof (uint8_t));
                loc += sizeof (uint8_t);

                //we write the pointer of the entry
                memcpy(loc, &(re->pointer), sizeof (int));
                loc += sizeof (int);
            } else {
                flag = 1;
                memcpy(loc, &flag, sizeof (uint8_t));
                loc += sizeof (uint8_t);

                //we write the pointer of the entry
                memcpy(loc, &(re->pointer), sizeof (int));
                loc += sizeof (int);

                //we write the bbox
                memcpy(loc, re->bbox, sizeof (BBox));
                loc += sizeof (BBox);
            }
        }
    } else {
        _DEBUGF(ERROR, "eFIND does not support this index (%d) yet.", index_type);
    }

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    if (base->gp->io_access == DIRECT_ACCESS)
        free(buf);
    else
        lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

void efind_write_log_del_node(const SpatialIndex *base, eFINDSpecification *spec,
        int node_page, int height) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t t = eFIND_STATUS_DEL;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
#endif

    //if log_size <= 0 then we do not have any control of data durability
    if (spec->log_size <= 0)
        return;

#ifdef COLLECT_STATISTICAL_DATA  
    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    bufsize = efind_size_of_del_node();

    //if we our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        efind_compact_log(base, spec);
    }

    if (base->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, base->gp->page_size, bufsize)) {
            _DEBUG(ERROR, "Allocation failed at execute_flushing");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(bufsize);
    }

    loc = buf;

    //now we have to serialize the offset of the previous element
    memcpy(loc, &(spec->offset_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    //now we serialize the type of modification
    memcpy(loc, &t, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //now we serialize the identifier of the node
    memcpy(loc, &node_page, sizeof (int));
    loc += sizeof (int);

    //now we serialize the height of the node
    memcpy(loc, &height, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    if (base->gp->io_access == DIRECT_ACCESS)
        free(buf);
    else
        lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

void efind_write_log_flush(const SpatialIndex *base, eFINDSpecification *spec,
        int* flushed_nodes, int n) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t t = eFIND_STATUS_FLUSH;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
#endif

    //if log_size <= 0 then we do not have any control of data durability
    if (spec->log_size <= 0)
        return;

#ifdef COLLECT_STATISTICAL_DATA  
    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    bufsize = efind_size_of_flushed_nodes(n);

    //if we our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        efind_compact_log(base, spec);
    }

    if (base->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, base->gp->page_size, bufsize)) {
            _DEBUG(ERROR, "Allocation failed at execute_flushing");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(bufsize);
    }

    loc = buf;

    //now we have to serialize the offset of the previous element
    memcpy(loc, &(spec->offset_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    //now we serialize the type of modification
    memcpy(loc, &t, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //now we serialize the quantity of pages that were flushed
    memcpy(loc, &n, sizeof (int));
    loc += sizeof (int);

    //now we serialize the flushed node identifiers
    memcpy(loc, flushed_nodes, sizeof (int) * n);
    loc += (sizeof (int) * n);

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    nof_flushing++;
    //_DEBUGF(NOTICE, "It registered a flushing and now nof_flushing is %d", nof_flushing);

    if (base->gp->io_access == DIRECT_ACCESS)
        free(buf);
    else
        lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

/*for the compaction and log recovery we need a stack
 the algorithm is the same of the FAST
 but, here, we handle our objects and entries of the buffers
 which are different from the FAST buffer and so on*/

typedef struct _fasinf_log_entry_item {
    eFINDLogEntry *l_entry;
    struct _fasinf_log_entry_item *next;
} eFINDStackItem;

typedef struct {
    eFINDStackItem *top;
    int size;
} eFINDLogRedoStack;

static eFINDLogRedoStack * efind_log_stack_init(void);

static void efind_log_stack_push(eFINDLogRedoStack *stack, eFINDLogEntry * l_entry);

/* this function returns a copy of the FASINFLogEntry and then destroy it from the stack*/
static eFINDLogEntry * efind_log_stack_pop(eFINDLogRedoStack *stack, uint8_t index_type);

/* free the stack*/
static void efind_log_stack_destroy(eFINDLogRedoStack *stack, uint8_t index_type);

eFINDLogRedoStack * efind_log_stack_init() {
    eFINDLogRedoStack *stack = (eFINDLogRedoStack*) lwalloc(sizeof (eFINDLogRedoStack));
    stack->top = NULL;
    stack->size = 0;
    return stack;
}

void efind_log_stack_push(eFINDLogRedoStack *stack, eFINDLogEntry * l_entry) {
    eFINDStackItem *tmp = (eFINDStackItem*) lwalloc(sizeof (eFINDStackItem));
    if (tmp == NULL) {
        _DEBUG(ERROR, "There is no memory to allocate in our stack of nodes");
    }
    tmp->l_entry = l_entry;
    tmp->next = stack->top;
    stack->top = tmp;
    stack->size++;
}

/* this function returns a pointer to eFINDLogEntry and then remove it from the stack*/
eFINDLogEntry *efind_log_stack_pop(eFINDLogRedoStack *stack, uint8_t index_type) {
    eFINDStackItem *it = stack->top;
    eFINDLogEntry *le;
    if (stack->top == NULL) {
        return NULL;
    }
    //this is the logentry that we will return
    le = it->l_entry;

    //advance the top of the stack
    stack->top = it->next;
    stack->size--;

    //an upper free
    lwfree(it);

    return le;
}

/* free the stack*/
void efind_log_stack_destroy(eFINDLogRedoStack * stack, uint8_t index_type) {
    eFINDStackItem *current;
    while (stack->top != NULL) {
        current = stack->top;
        stack->top = current->next;
        
        efind_log_entry_free(current->l_entry, index_type);
        lwfree(current);
    }
    lwfree(stack);
}

/*this is similar to the recovery_efind_log!*/
void efind_compact_log(const SpatialIndex *base, eFINDSpecification * spec) {
    int *flushed_nodes = NULL; //an auxiliary array
    int n_fn = 0; //the total elements in flushed_nodes
    int i; //for iterate arrays
    int j = 0; //the current index of flushed_nodes
    int r;
    //size of the last element, offset of the last element, offset of the previous element
    size_t size_last_entry, offset_last_entry, offset_previous_entry;
    uint8_t *buf;
    eFINDLogEntry *le;
    eFINDLogRedoStack *stack;
    char *temp_f; //the new log file
    char *old_log; //the old log file to be erased from disk/flash memory
    uint8_t index_type;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    /*
     * we only execute the compaction log if at least a minimum number of flushing operations was previously performed
     */
    if (nof_flushing < min_nof_flushing) {
        //we do nothing        
        return;
    }
    //_DEBUGF(NOTICE, "We will compact the log %s because there nof_flushing is %d and min_nof_flushing is %d",
    //        spec->log_file, nof_flushing, min_nof_flushing);

    /*if number of flushing is in its initial state (i.e., -1) or greater than 0...
     we execute the compactor*/

    index_type = spatialindex_get_type(base);

    //we do not have offset
    if (spec->offset_last_elem_log == -1) {
        _DEBUG(ERROR, "We do not have the last offset of the log file");
        return;
    }

    offset_last_entry = spec->offset_last_elem_log;
    size_last_entry = spec->size_last_elem_log;

    stack = efind_log_stack_init();

    flushed_nodes = (int*) lwalloc(sizeof (int));

    //while the current entry is not the first one
    while (offset_last_entry != 0) {
        //we firstly read the current last entry from log
        buf = (uint8_t*) lwalloc(size_last_entry);
        raw_read_log(spec->log_file, offset_last_entry, buf, size_last_entry);
        //we deserialize and get the offset for the previous log entry
        le = efind_retrieve_log_entry(buf, &offset_previous_entry, index_type);

        //if we have some flushed nodes
        if (le->status == eFIND_STATUS_FLUSH) {
            //we update the total number of flushed nodes
            n_fn += le->value.flushed_nodes->n;
            flushed_nodes = (int*) lwrealloc(flushed_nodes, sizeof (int)*n_fn);
            //we add these flushed nodes
            for (i = 0; i < le->value.flushed_nodes->n; i++) {
                flushed_nodes[j] = le->value.flushed_nodes->pages_id[i];
                j++;
            }
            //we free the le
            efind_log_entry_free(le, index_type);
        } else {
            //if this entry was not previously flushed, then we stored it in our stack
            if (!array_contains_element(flushed_nodes, n_fn, le->page_id)) {
                efind_log_stack_push(stack, le);
            } else {
                efind_log_entry_free(le, index_type);
            }
        }
        size_last_entry = offset_last_entry - offset_previous_entry;
        offset_last_entry = offset_previous_entry;
        lwfree(buf);
        buf = NULL;
        le = NULL;
    }
    lwfree(flushed_nodes);

    /*this never will be the case, but it is here for checking
     * */
    if (n_fn == 0) {
        efind_log_stack_destroy(stack, index_type);
        _DEBUG(WARNING, "Wow, it is not possible to compact the log because"
                " there is no flushed nodes");
        return;
        /*_DEBUGF(WARNING, "Wow. The maximum log size %zd is insufficient for this parameterization. "
                "Current buffer size %d and the maximum buffer size is %zd. "
                "This means that there is no flushed nodes to be compacted in the log.",
                spec->log_size, _cur_buffer_size, spec->write_buffer_size);*/
    }

    //because there is not flushing 
    nof_flushing = 0;

    //we start to copy the not flushed entries into another version of the log
    temp_f = (char*) lwalloc(strlen(spec->log_file) + strlen(".tmp") + 1);
    sprintf(temp_f, "%s.tmp", spec->log_file);

    old_log = (char*) lwalloc(strlen(spec->log_file) + 1);
    strcpy(old_log, spec->log_file);

    lwfree(spec->log_file);
    spec->log_file = temp_f;

    spec->offset_last_elem_log = 0;
    spec->size_last_elem_log = 0;

    /*note that we reuse log functions here */
    while (stack->size > 0) {
        le = efind_log_stack_pop(stack, index_type);

        if (le->status == eFIND_STATUS_DEL) {
            efind_write_log_del_node(base, spec, le->page_id, le->height);
        } else if (le->status == eFIND_STATUS_NEW) {
            efind_write_log_create_node(base, spec, le->page_id, le->height);
        } else if (le->status == eFIND_STATUS_MOD) {
            efind_write_log_mod_node(base, spec, le->page_id,
                    (void *) le->value.mod->entry, le->height);
        }
        efind_log_entry_free(le, index_type);
    }
    efind_log_stack_destroy(stack, index_type);

    //we remove the last version of the log file
    remove(old_log);

    //we back to the original name since the compaction is completed
    r = rename(spec->log_file, old_log);
    if (r == 0) {
        lwfree(spec->log_file);
        spec->log_file = old_log;
    } else {
        lwfree(old_log);
    }

    //_DEBUGF(NOTICE, "Now the log file is %s", spec->log_file);

    /*
     now we check if the compaction reduced the size of the log to satisfy the maximum log size
     */
    if (spec->offset_last_elem_log + spec->size_last_elem_log > spec->log_size) {
        /*in the negative case, we allow that the log stores more modifications
    and that the minimum number of flushing operations be increased
    with this increasing, we try to better compact the log in future compactions*/
        min_nof_flushing++;
    } else {
        /* we return the default behavior of the compaction log*/
        min_nof_flushing = 1;
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _compactation_log_num++;

    _compactation_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _compactation_log_time += get_elapsed_time(start, end);
#endif
}

void efind_recovery_log(const SpatialIndex *base, eFINDSpecification * spec) {
    int *flushed_nodes = NULL; //an auxiliary array
    int n_fn = 0; //the total elements in flushed_nodes
    int i; //for iterate arrays
    int j = 0; //the current index of flushed_nodes
    int r;
    //size of the last element, offset of the last element, offset of the previous element
    size_t size_last_entry, offset_last_entry, offset_previous_entry;
    uint8_t *buf;
    eFINDLogEntry *le;
    eFINDLogRedoStack *stack;
    char *temp_f; //the new log file
    char *old_log; //the old log file to be erased from disk/flash memory
    uint8_t index_type;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    index_type = spatialindex_get_type(base);

    //we do not have offset
    if (spec->offset_last_elem_log == -1)
        return;

    offset_last_entry = spec->offset_last_elem_log;
    size_last_entry = spec->size_last_elem_log;

    stack = efind_log_stack_init();

    flushed_nodes = (int*) lwalloc(sizeof (int));

    //while the current entry is not the first one
    while (offset_last_entry != 0) {
        //we firstly read the current last entry from log
        buf = (uint8_t*) lwalloc(size_last_entry);
        raw_read_log(spec->log_file, offset_last_entry, buf, size_last_entry);
        //we deserialize and get the offset for the previous log entry
        le = efind_retrieve_log_entry(buf, &offset_previous_entry, index_type);

        //if we have some flushed nodes
        if (le->status == eFIND_STATUS_FLUSH) {
            //we update the total number of flushed nodes
            n_fn += le->value.flushed_nodes->n;
            flushed_nodes = (int*) lwrealloc(flushed_nodes, sizeof (int)*n_fn);
            //we add the flushed node into the global flushed nodes
            for (i = 0; i < le->value.flushed_nodes->n; i++) {
                flushed_nodes[j] = le->value.flushed_nodes->pages_id[i];
                j++;
            }
            //we free the le
            efind_log_entry_free(le, index_type);
        } else {
            //if this entry was not previously flushed, then we stored it in our stack
            if (!array_contains_element(flushed_nodes, n_fn, le->page_id)) {
                efind_log_stack_push(stack, le);
            } else {
                efind_log_entry_free(le, index_type);
            }
        }
        size_last_entry = offset_last_entry - offset_previous_entry;
        offset_last_entry = offset_previous_entry;
        lwfree(buf);
        buf = NULL;
        le = NULL;
    }
    lwfree(flushed_nodes);

    //we start to copy the not flushed entries into another version of the log
    temp_f = (char*) lwalloc(sizeof (spec->log_file) + sizeof (".tmp"));
    sprintf(temp_f, "%s.tmp", spec->log_file);

    old_log = (char*) lwalloc(sizeof (spec->log_file));
    strcpy(old_log, spec->log_file);

    lwfree(spec->log_file);
    spec->log_file = temp_f;

    spec->offset_last_elem_log = 0;
    spec->size_last_elem_log = 0;

    /*note that the fb_* functions will create a new log file and reconstruct the buffer */
    while (stack->size > 0) {
        le = efind_log_stack_pop(stack, index_type);
        if (le->status == eFIND_STATUS_DEL) {
            efind_buf_del_node(base, spec, le->page_id, le->height);
        } else if (le->status == eFIND_STATUS_NEW) {
            efind_buf_create_node(base, spec, le->page_id, le->height);
        } else if (le->status == eFIND_STATUS_MOD) {
            if (le->value.mod->entry != NULL) {
                if (index_type == eFIND_RTREE_TYPE || index_type == eFIND_RSTARTREE_TYPE
                        || (index_type == eFIND_HILBERT_RTREE_TYPE && le->height == 0)) {
                    efind_buf_mod_node(base, spec, le->page_id, rentry_clone((REntry*) le->value.mod->entry), le->height);
                } else {
                    efind_buf_mod_node(base, spec, le->page_id, hilbertientry_clone((HilbertIEntry*) le->value.mod->entry), le->height);
                }
            } else {
                efind_buf_mod_node(base, spec, le->page_id, le->value.mod->entry, le->height);
            }
        }
        efind_log_entry_free(le, index_type);
    }
    efind_log_stack_destroy(stack, index_type);

    //we remove the last version of the log file
    remove(old_log);

    //we back to the original name since the compaction is done
    r = rename(spec->log_file, old_log);
    if (r == 0) {
        lwfree(spec->log_file);
        spec->log_file = old_log;
    } else {
        lwfree(old_log);
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _recovery_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _recovery_log_time += get_elapsed_time(start, end);
#endif
}
