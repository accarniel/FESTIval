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

#include <sys/types.h>  /* required by open() */
#include <unistd.h>     /* open(), write() */
#include <fcntl.h>      /* open() and fcntl() */
#include <string.h>
#include <stringbuffer.h> /* for memcpy */

#include "fast_log_module.h" //basic functions

#include "../main/io_handler.h" //for io operations
#include "fast_redo_stack.h" //stack
#include "fast_buffer.h" //buffer operation
#include "fast_flush_module.h"
#include "fast_buffer_list_mod.h" //for the identifiers
#include "../main/log_messages.h" //debug messages

#include "../main/statistical_processing.h"

uint8_t is_compacting = 0;

static void raw_write_log(const char *file, uint8_t *buf, size_t bufsize);
static void raw_read_log(const char *file, size_t offset, uint8_t *buf, size_t bufsize);
/*this function process the retrieved entry of the log 
 * it push in the redostack */
//specify it for each type of entry.... or create a container for it
static LogEntry *retrieve_log_entry(uint8_t *buf, size_t *offset, uint8_t index_type);

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

void log_entry_free(LogEntry* le, uint8_t index_type) {
    if (le->status == FAST_STATUS_NEW) {
        if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
            rnode_free((RNode *) le->value.node);
        } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
            hilbertnode_free((HilbertRNode *) le->value.node);
        }
    } else if (le->status == FAST_STATUS_MOD) {
        if (le->value.mod->type == FAST_ITEM_TYPE_K) {
            if (le->value.mod->value.bbox != NULL) {
                lwfree(le->value.mod->value.bbox);
            }
        }
        lwfree(le->value.mod);
    } else if (le->status == FAST_STATUS_FLUSH) {
        if (le->value.flushed_nodes->node_pages != NULL)
            lwfree(le->value.flushed_nodes->node_pages);
        lwfree(le->value.flushed_nodes);
    } else {
        if (le->status != FAST_STATUS_DEL)
            _DEBUGF(ERROR, "Unknown status in the log file: %d", le->status);
    }
    lwfree(le);
}

static size_t size_of_new_node(void * node, uint8_t index_type);
static size_t size_of_pointer_mod(void);
static size_t size_of_lhv_mod(void); //only for Hilbert R-trees
static size_t size_of_bbox_mod(BBox *bbox);
static size_t size_of_del_node(void);
static size_t size_of_hole_mod(void);
static size_t size_of_flushed_nodes(int n);

size_t size_of_new_node(void * node, uint8_t index_type) {
    size_t bufsize;
    //the size of previous element in the log file and the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
        //size of the node plus the identifier of the node
        bufsize += rnode_size((RNode *) node) + sizeof (int);
    } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
        //size of the node plus the identifier of the node
        bufsize += hilbertnode_size((HilbertRNode *) node) + sizeof (int);
    }
    //size of the rnode height
    bufsize += sizeof (int);

    return bufsize;
}

size_t size_of_bbox_mod(BBox * bbox) {
    size_t bufsize;
    //the size of previous element in the log file and the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    //rnode_page, type of modification, position, and the flag
    bufsize += sizeof (int) + sizeof (uint8_t) + sizeof (uint32_t) + sizeof (uint8_t);
    //size of the rnode height
    bufsize += sizeof (int);

    if (bbox != NULL) {
        bufsize += sizeof (BBox);
    }

    return bufsize;
}

size_t size_of_pointer_mod() {
    size_t bufsize;
    //the size of previous element in the log file and the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    //size of the rnode_page, type of the modification, position, the new pointer
    bufsize += sizeof (int) + sizeof (uint8_t) + sizeof (uint32_t) + sizeof (uint32_t);
    //size of the rnode height
    bufsize += sizeof (int);
    return bufsize;
}

size_t size_of_hole_mod() {
    size_t bufsize;
    //the size of previous element in the log file and the type of mod
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    bufsize += sizeof (int) + sizeof (uint8_t) + sizeof (uint32_t);
    bufsize += sizeof (int);
    return bufsize;
}

size_t size_of_lhv_mod() {
    size_t bufsize;
    //the size of previous element in the log file and the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    //size of the node_page, type of the modification, position, the new lhv
    bufsize += sizeof (int) + sizeof (uint8_t) + sizeof (uint32_t) + sizeof (hilbert_value_t);
    //size of the node height
    bufsize += sizeof (int);
    return bufsize;
}

size_t size_of_del_node() {
    size_t bufsize;
    //the size of previous element in the log file and the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    //size of the rnode_page
    bufsize += sizeof (int);
    //size of the rnode height
    bufsize += sizeof (int);
    return bufsize;
}

size_t size_of_flushed_nodes(int n) {
    size_t bufsize;
    //the size of previous element in the log file and the type of modification
    bufsize = sizeof (size_t) + sizeof (uint8_t);
    //size of the flushed_nodes
    bufsize += sizeof (int) + (sizeof (int) * n);
    return bufsize;
}

LogEntry * retrieve_log_entry(uint8_t *buf, size_t * offset, uint8_t index_type) {
    LogEntry *ret;
    size_t p;

    if (buf == NULL) {
        _DEBUG(ERROR, "Buffer is null in retrieve_log_entry");
        return NULL;
    }

    ret = (LogEntry*) lwalloc(sizeof (LogEntry));

    memcpy(&p, buf, sizeof (size_t));
    buf += sizeof (size_t);

    memcpy(&(ret->status), buf, sizeof (uint8_t));
    buf += sizeof (uint8_t);

    if (ret->status == FAST_STATUS_NEW) {
        int n, i;

        memcpy(&(ret->node_page), buf, sizeof (int));
        buf += sizeof (int);

        memcpy(&(ret->node_height), buf, sizeof (int));
        buf += sizeof (int);

        if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
            RNode *rnode = rnode_create_empty();

            memcpy(&n, buf, sizeof (uint32_t));
            buf += sizeof (uint32_t);

            rnode->nofentries = n;
            rnode->entries = (REntry**) lwalloc(sizeof (REntry*) * n);

            for (i = 0; i < n; i++) {
                rnode->entries[i] = (REntry*) lwalloc(sizeof (REntry));
                rnode->entries[i]->bbox = (BBox*) lwalloc(sizeof (BBox));

                memcpy(&(rnode->entries[i]->pointer), buf, sizeof (uint32_t));
                buf += sizeof (uint32_t);

                memcpy(rnode->entries[i]->bbox, buf, sizeof (BBox));
                buf += sizeof (BBox);
            }
            ret->value.node = (void *) rnode;
        } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
            HilbertRNode *hilbertnode = (HilbertRNode*) lwalloc(sizeof (HilbertRNode));

            memcpy(&n, buf, sizeof (uint32_t));
            buf += sizeof (uint32_t);

            memcpy(&hilbertnode->type, buf, sizeof (uint8_t));
            buf += sizeof (uint8_t);

            hilbertnode->nofentries = n;

            if (hilbertnode->type == HILBERT_INTERNAL_NODE) {
                hilbertnode->entries.internal = (HilbertIEntry**) lwalloc(sizeof (HilbertIEntry*) * n);
                for (i = 0; i < n; i++) {
                    hilbertnode->entries.internal[i] = (HilbertIEntry*) lwalloc(sizeof (HilbertIEntry));
                    hilbertnode->entries.internal[i]->bbox = (BBox*) lwalloc(sizeof (BBox));

                    memcpy(&(hilbertnode->entries.internal[i]->pointer), buf, sizeof (uint32_t));
                    buf += sizeof (uint32_t);

                    memcpy(&(hilbertnode->entries.internal[i]->lhv), buf, sizeof (hilbert_value_t));
                    buf += sizeof (hilbert_value_t);

                    memcpy(hilbertnode->entries.internal[i]->bbox, buf, sizeof (BBox));
                    buf += sizeof (BBox);
                }
            } else {
                hilbertnode->entries.leaf = (REntry**) lwalloc(sizeof (REntry*) * n);
                for (i = 0; i < n; i++) {
                    hilbertnode->entries.leaf[i] = (REntry*) lwalloc(sizeof (REntry));
                    hilbertnode->entries.leaf[i]->bbox = (BBox*) lwalloc(sizeof (BBox));

                    memcpy(&(hilbertnode->entries.leaf[i]->pointer), buf, sizeof (uint32_t));
                    buf += sizeof (uint32_t);

                    memcpy(hilbertnode->entries.leaf[i]->bbox, buf, sizeof (BBox));
                    buf += sizeof (BBox);
                }
            }
            ret->value.node = (void *) hilbertnode;
        }
    } else if (ret->status == FAST_STATUS_MOD) {
        memcpy(&(ret->node_page), buf, sizeof (int));
        buf += sizeof (int);

        memcpy(&(ret->node_height), buf, sizeof (int));
        buf += sizeof (int);

        ret->value.mod = (LogMOD*) lwalloc(sizeof (LogMOD));

        memcpy(&(ret->value.mod->type), buf, sizeof (uint8_t));
        buf += sizeof (uint8_t);

        /* now we read the position*/
        memcpy(&(ret->value.mod->position), buf, sizeof (uint32_t));
        buf += sizeof (uint32_t);

        if (ret->value.mod->type == FAST_ITEM_TYPE_K) {
            uint8_t flag;

            memcpy(&flag, buf, sizeof (uint8_t));
            buf += sizeof (uint8_t);

            if (flag == 0) {
                ret->value.mod->value.bbox = NULL;
            } else {
                ret->value.mod->value.bbox = (BBox*) lwalloc(sizeof (BBox));

                memcpy(ret->value.mod->value.bbox, buf, sizeof (BBox));
                buf += sizeof (BBox);
            }
        } else if (ret->value.mod->type == FAST_ITEM_TYPE_P) {
            memcpy(&(ret->value.mod->value.pointer), buf, sizeof (uint32_t));
            buf += sizeof (uint32_t);
        } else if (ret->value.mod->type == FAST_ITEM_TYPE_L) {
            memcpy(&(ret->value.mod->value.lhv), buf, sizeof (hilbert_value_t));
            buf += sizeof (hilbert_value_t);
        } else if (ret->value.mod->type != FAST_ITEM_TYPE_H) {
            _DEBUGF(ERROR, "Unknown type of modification (%d) at log entry", ret->value.mod->type);
            return NULL;
        }
    } else if (ret->status == FAST_STATUS_DEL) {
        memcpy(&(ret->node_page), buf, sizeof (uint32_t));
        buf += sizeof (uint32_t);

        memcpy(&(ret->node_height), buf, sizeof (uint32_t));
        buf += sizeof (uint32_t);
    } else if (ret->status == FAST_STATUS_FLUSH) {
        int n, i;
        ret->value.flushed_nodes = (FlushedNodes*) lwalloc(sizeof (FlushedNodes));

        memcpy(&n, buf, sizeof (int));
        buf += sizeof (int);

        ret->value.flushed_nodes->n = n;
        if (n <= 0) {
            _DEBUG(WARNING, "There is a flushed node without node in log...");
            ret->value.flushed_nodes->node_pages = NULL;
        } else {
            ret->value.flushed_nodes->node_pages = (int*) lwalloc(sizeof (int)*n);
        }

        for (i = 0; i < n; i++) {
            memcpy(&(ret->value.flushed_nodes->node_pages[i]), buf, sizeof (int));
            buf += sizeof (int);
        }
    } else {
        _DEBUGF(ERROR, "Unknown status in the log file: %d", ret->status);
        return NULL;
    }

    if (offset)
        *offset = p;

    return ret;
}

void write_log_new_node(const SpatialIndex *base, FASTSpecification *spec,
        int new_node_page, void *new_node, int height) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t t = FAST_STATUS_NEW;
    uint8_t index_type;
    size_t nodesize;
    uint8_t *serialized_node;
    uint8_t *loc_node;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    index_type = spatialindex_get_type(base);

    bufsize = size_of_new_node(new_node, index_type);

    nodesize = bufsize - (sizeof (int) + sizeof (int) + sizeof (size_t) + sizeof (uint8_t));
    serialized_node = (uint8_t*) lwalloc(nodesize);
    loc_node = serialized_node;

    //here, we serialize the new node before the compaction
    //it is needed for those cases if the log is full and this node is flushed by an emergency flushing    

    if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
        rnode_serialize((RNode *) new_node, loc_node);
    } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
        hilbertnode_serialize((HilbertRNode*) new_node, loc_node);
    }

    //if our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        compact_fast_log(base, spec);
    }

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //now we have to serialize the offset of the previous element
    memcpy(loc, &(spec->offset_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    //now we serialize the type of modification
    memcpy(loc, &t, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //now we serialize the identifier of the node
    memcpy(loc, &new_node_page, sizeof (int));
    loc += sizeof (int);

    //now we serialize the height of the node
    memcpy(loc, &height, sizeof (int));
    loc += sizeof (int);

    //now the node
    memcpy(loc, serialized_node, nodesize);

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    lwfree(buf);
    lwfree(serialized_node);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

void write_log_mod_bbox(const SpatialIndex *base, FASTSpecification *spec,
        int node_page, BBox *new_bbox, int position, int height) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t t = FAST_STATUS_MOD;
    uint8_t tm = FAST_ITEM_TYPE_K;
    uint8_t flag;
    BBox *b;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    if (new_bbox == NULL) {
        b = NULL;
    } else {
        b = bbox_clone(new_bbox);
    }

    bufsize = size_of_bbox_mod(b);

    //if we our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        compact_fast_log(base, spec);
    }

    buf = (uint8_t*) lwalloc(bufsize);

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
    memcpy(loc, &height, sizeof (int));
    loc += sizeof (int);

    //now we serialize the type of modification (p or k)
    memcpy(loc, &tm, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* now we have to serialize the position*/
    memcpy(loc, &position, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    if (b == NULL) {
        flag = 0;
        memcpy(loc, &flag, sizeof (uint8_t));
        loc += sizeof (uint8_t);
    } else {
        flag = 1;
        memcpy(loc, &flag, sizeof (uint8_t));
        loc += sizeof (uint8_t);

        memcpy(loc, b, sizeof (BBox));
        loc += sizeof (BBox);
    }
    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    lwfree(buf);
    lwfree(b);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

void write_log_mod_pointer(const SpatialIndex *base, FASTSpecification *spec,
        int node_page, int new_pointer, int position, int height) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t tm = FAST_ITEM_TYPE_P;
    uint8_t t = FAST_STATUS_MOD;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    bufsize = size_of_pointer_mod();

    //if we our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        compact_fast_log(base, spec);
    }

    buf = (uint8_t*) lwalloc(bufsize);

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
    memcpy(loc, &height, sizeof (int));
    loc += sizeof (int);

    //now we serialize the type of modification (p or k)
    memcpy(loc, &tm, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* now we have to serialize the position*/
    memcpy(loc, &position, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    /* now we have to serialize the pointer*/
    memcpy(loc, &new_pointer, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

void write_log_mod_lhv(const SpatialIndex *base, FASTSpecification *spec,
        int node_page, hilbert_value_t new_lhv, int position, int height) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t tm = FAST_ITEM_TYPE_L;
    uint8_t t = FAST_STATUS_MOD;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    bufsize = size_of_lhv_mod();

    //if we our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        compact_fast_log(base, spec);
    }

    buf = (uint8_t*) lwalloc(bufsize);

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
    memcpy(loc, &height, sizeof (int));
    loc += sizeof (int);

    //now we serialize the type of modification (p or k or l)
    memcpy(loc, &tm, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* now we have to serialize the position*/
    memcpy(loc, &position, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    /* now we have to serialize the LHV*/
    memcpy(loc, &new_lhv, sizeof (hilbert_value_t));
    loc += sizeof (hilbert_value_t);

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

void write_log_mod_hole(const SpatialIndex *base, FASTSpecification *spec,
        int node_page, int position, int height) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t tm = FAST_ITEM_TYPE_H;
    uint8_t t = FAST_STATUS_MOD;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    bufsize = size_of_hole_mod();

    //if we our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        compact_fast_log(base, spec);
    }

    buf = (uint8_t*) lwalloc(bufsize);

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
    memcpy(loc, &height, sizeof (int));
    loc += sizeof (int);

    //now we serialize the type of modification 
    memcpy(loc, &tm, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    /* now we have to serialize the position*/
    memcpy(loc, &position, sizeof (uint32_t));
    loc += sizeof (uint32_t);

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

void write_log_del_node(const SpatialIndex *base, FASTSpecification *spec,
        int node_page, int height) {
    uint8_t *loc;
    uint8_t *buf;
    size_t bufsize;
    uint8_t t = FAST_STATUS_DEL;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    bufsize = size_of_del_node();

    //if we our log does not have space, we need to compact it
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size) {
        compact_fast_log(base, spec);
    }

    buf = (uint8_t*) lwalloc(bufsize);

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
    memcpy(loc, &height, sizeof (int));
    loc += sizeof (int);

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

void write_log_flush(const SpatialIndex *base, FASTSpecification *spec, int* flushed_nodes, int n) {
    uint8_t *loc;
    uint8_t *buf;
    int i;
    size_t bufsize;
    uint8_t t = FAST_STATUS_FLUSH;

#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    bufsize = size_of_flushed_nodes(n);

    //if we our log does not have space, we need to compact it
    //we only compact it if it is not being calling by the compaction log
    if (spec->offset_last_elem_log + spec->size_last_elem_log + bufsize > spec->log_size &&
            is_compacting == 0) {
        compact_fast_log(base, spec);
    }

    buf = (uint8_t*) lwalloc(bufsize);

    loc = buf;

    //now we have to serialize the offset of the previous element
    memcpy(loc, &(spec->offset_last_elem_log), sizeof (size_t));
    loc += sizeof (size_t);

    //now we serialize the type of modification
    memcpy(loc, &t, sizeof (uint8_t));
    loc += sizeof (uint8_t);

    //now we serialize the quantity of nodes that were flushed
    memcpy(loc, &n, sizeof (int));
    loc += sizeof (int);

    for (i = 0; i < n; i++) {
        //now we serialize the identifier of the node which was flushed
        memcpy(loc, &(flushed_nodes[i]), sizeof (int));
        loc += sizeof (int);
    }

    //we sequentially write it
    raw_write_log(spec->log_file, buf, bufsize);
    //we update the information
    spec->offset_last_elem_log += spec->size_last_elem_log;
    spec->size_last_elem_log = bufsize;

    lwfree(buf);

#ifdef COLLECT_STATISTICAL_DATA
    _cur_log_size = spec->offset_last_elem_log + spec->size_last_elem_log;

    cpuend = get_CPU_time();
    end = get_current_time();

    _write_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _write_log_time += get_elapsed_time(start, end);
#endif
}

/*this is similar to the recovery_fast_log!*/
void compact_fast_log(const SpatialIndex *base, FASTSpecification * spec) {
    int *flushed_nodes = NULL; //an auxiliary array
    int n_fn = 0; //the total elements in flushed_nodes
    int i; //for iterate arrays
    int j = 0; //the current index of flushed_nodes
    int r;
    //size of the last element, offset of the last element, offset of the previous element
    size_t size_last_entry, offset_last_entry, offset_previous_entry;
    uint8_t *buf;
    LogEntry *le;
    RedoStack *stack;
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

    _compactation_log_num++;
#endif

    index_type = spatialindex_get_type(base);

    //we do not have offset
    if (spec->offset_last_elem_log == -1) {
        _DEBUG(ERROR, "We do not have the last offset of the log file");
        return;
    }

    offset_last_entry = spec->offset_last_elem_log;
    size_last_entry = spec->size_last_elem_log;

    stack = redostack_init();

    flushed_nodes = (int*) lwalloc(sizeof (int));

    //while the current entry is not the first one
    while (offset_last_entry != 0) {

        //we firstly read the current last entry from log
        buf = (uint8_t*) lwalloc(size_last_entry);
        raw_read_log(spec->log_file, offset_last_entry, buf, size_last_entry);
        //we deserialize and get the offset for the previous log entry
        le = retrieve_log_entry(buf, &offset_previous_entry, index_type);

        //if we have some flushed nodes
        if (le->status == FAST_STATUS_FLUSH) {
            //we update the total number of flushed nodes
            n_fn += le->value.flushed_nodes->n;
            flushed_nodes = (int*) lwrealloc(flushed_nodes, sizeof (int)*n_fn);
            //we add these flushed nodes
            for (i = 0; i < le->value.flushed_nodes->n; i++) {
                flushed_nodes[j] = le->value.flushed_nodes->node_pages[i];
                j++;
            }
            //we free the le
            log_entry_free(le, index_type);
        } else {
            //if this entry was not previously flushed, then we stored it in our stack
            if (!array_contains_element(flushed_nodes, n_fn, le->node_page)) {
                redostack_push(stack, le);
            } else {
                log_entry_free(le, index_type);
            }
        }
        size_last_entry = offset_last_entry - offset_previous_entry;
        offset_last_entry = offset_previous_entry;
        lwfree(buf);
        buf = NULL;
        le = NULL;
    }
    lwfree(flushed_nodes);

    /*if we have not flushed node, then we have to adjust the size of log
     * since the original paper of FAST does not automatically adjust the size of log,
     * we return an ERROR
     * */
    if (n_fn == 0) {
        redostack_destroy(stack, index_type);
        //_DEBUGF(ERROR, "Maximum log size %zd is insufficient for this parameterization. "
        //        "Current buffer size %d and the maximum buffer size is %zd. "
        //        "This means that there is no flushed nodes to be compacted in the log.",
        //        spec->log_size, _cur_buffer_size, spec->buffer_size);
        //the description of the compaction makes a comment about calling an emergence flushing operation
        if (!is_processing_hole()) { //we only process a flushing if we are not processing a hole (for FAST Hilbert R-tree)
            is_compacting = 1;

            fast_execute_flushing(base, spec);

            is_compacting = 0;
        }
        return;
    }

    //we start to copy the not flushed entries into another version of the log
    temp_f = (char*) lwalloc(strlen(spec->log_file) + strlen(".tmp") + 1);
    sprintf(temp_f, "%s.tmp", spec->log_file);

    old_log = (char*) lwalloc(strlen(spec->log_file) + 1);
    strcpy(old_log, spec->log_file);

    lwfree(spec->log_file);
    spec->log_file = temp_f;

    spec->offset_last_elem_log = 0;
    spec->size_last_elem_log = 0;

    /*note that the we reuse log functions here */
    while (stack->size > 0) {
        le = redostack_pop(stack, index_type);

        if (le->status == FAST_STATUS_DEL) {
            write_log_del_node(base, spec, le->node_page, le->node_height);
        } else if (le->status == FAST_STATUS_NEW) {
            write_log_new_node(base, spec, le->node_page, le->value.node, le->node_height);
        } else if (le->status == FAST_STATUS_MOD) {
            if (le->value.mod->type == FAST_ITEM_TYPE_K) {
                write_log_mod_bbox(base, spec, le->node_page, le->value.mod->value.bbox,
                        le->value.mod->position, le->node_height);
            } else if (le->value.mod->type == FAST_ITEM_TYPE_P) {
                write_log_mod_pointer(base, spec, le->node_page, le->value.mod->value.pointer,
                        le->value.mod->position, le->node_height);
            } else if (le->value.mod->type == FAST_ITEM_TYPE_L) {
                write_log_mod_lhv(base, spec, le->node_page, le->value.mod->value.lhv,
                        le->value.mod->position, le->node_height);
            } else if (le->value.mod->type == FAST_ITEM_TYPE_H) {
                write_log_mod_hole(base, spec, le->node_page, le->value.mod->position, le->node_height);
            }
        }
        log_entry_free(le, index_type);
    }
    redostack_destroy(stack, index_type);

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

    _compactation_log_cpu_time += get_elapsed_time(cpustart, cpuend);
    _compactation_log_time += get_elapsed_time(start, end);
#endif
}

void recovery_fast_log(const SpatialIndex *base, FASTSpecification * spec) {
    int *flushed_nodes = NULL; //an auxiliary array
    int n_fn = 0; //the total elements in flushed_nodes
    int i; //for iterate arrays
    int j = 0; //the current index of flushed_nodes
    int r;
    //size of the last element, offset of the last element, offset of the previous element
    size_t size_last_entry, offset_last_entry, offset_previous_entry;
    uint8_t *buf;
    LogEntry *le;
    RedoStack *stack;
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

    stack = redostack_init();

    flushed_nodes = (int*) lwalloc(sizeof (int));

    //while the current entry is not the first one
    while (offset_last_entry != 0) {
        //we firstly read the current last entry from log
        buf = (uint8_t*) lwalloc(size_last_entry);
        raw_read_log(spec->log_file, offset_last_entry, buf, size_last_entry);
        //we deserialize and get the offset for the previous log entry
        le = retrieve_log_entry(buf, &offset_previous_entry, index_type);

        //if we have some flushed nodes
        if (le->status == FAST_STATUS_FLUSH) {
            //we update the total number of flushed nodes
            n_fn += le->value.flushed_nodes->n;
            flushed_nodes = (int*) lwrealloc(flushed_nodes, sizeof (int)*n_fn);
            //we add the flushed node into the global flushed nodes
            for (i = 0; i < le->value.flushed_nodes->n; i++) {
                flushed_nodes[j] = le->value.flushed_nodes->node_pages[i];
                j++;
            }
            //we free the le
            log_entry_free(le, index_type);
        } else {
            //if this entry was not previously flushed, then we stored it in our stack
            if (!array_contains_element(flushed_nodes, n_fn, le->node_page)) {
                redostack_push(stack, le);
            } else {
                log_entry_free(le, index_type);
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
        le = redostack_pop(stack, index_type);
        if (le->status == FAST_STATUS_DEL) {
            fb_del_node(base, spec, le->node_page, le->node_height);
        } else if (le->status == FAST_STATUS_NEW) {
            if (le->value.node != NULL) {
                if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                    fb_put_new_node(base, spec, le->node_page, rnode_clone((RNode*) le->value.node), le->node_height);
                } else {
                    fb_put_new_node(base, spec, le->node_page, hilbertnode_clone((HilbertRNode*) le->value.node), le->node_height);
                }
            }
        } else if (le->status == FAST_STATUS_MOD) {
            if (le->value.mod->type == FAST_ITEM_TYPE_K) {
                if (le->value.mod->value.bbox != NULL) {
                    fb_put_mod_bbox(base, spec, le->node_page,
                            bbox_clone(le->value.mod->value.bbox), le->value.mod->position, le->node_height);
                } else {
                    fb_put_mod_bbox(base, spec, le->node_page,
                            NULL, le->value.mod->position, le->node_height);
                }
            } else if (le->value.mod->type == FAST_ITEM_TYPE_P) {
                fb_put_mod_pointer(base, spec, le->node_page,
                        le->value.mod->value.pointer, le->value.mod->position, le->node_height);
            } else if (le->value.mod->type == FAST_ITEM_TYPE_L) {
                fb_put_mod_lhv(base, spec, le->node_page,
                        le->value.mod->value.lhv, le->value.mod->position, le->node_height);
            } else if (le->value.mod->type == FAST_ITEM_TYPE_H) {
                fb_put_mod_hole(base, spec, le->node_page, le->value.mod->position, le->node_height);
            }
        }
        log_entry_free(le, index_type);
    }
    redostack_destroy(stack, index_type);

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
