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
 * This file implements the full version of the 2Q cache management proposed in
 * 
 * Reference: JOHNSON, T.; SHASHA, D. 2Q: A Low Overhead High Performance 
 * Buffer Management Replacement Algorithm. In Proceedings of the 
 * 20th International Conference on Very Large Data Bases (VLDB '94), p. 439-450, 1994.
 * 
 */

#include "buffer_handler.h"
#include "../libraries/uthash/uthash.h" //for hashing structures
#include "../main/log_messages.h" //for messages

#include "../main/statistical_processing.h" //for collection of statistical data

#include <stringbuffer.h> /* for printing */

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)


//this corresponds to the Am part of the 2Q, which is managed as a LRU cache

typedef struct {
    UT_hash_handle hh;

    int page_id; //the key
    uint8_t *data; //the serialized node
    bool modified; //this node has modification to be applied?
} Am; //it stores the most frequent accessed pages

//this corresponds to the A1in part of the 2Q, which is managed as a FIFO

typedef struct {
    UT_hash_handle hh;

    int page_id; //the key    
    uint8_t *data; //the serialized node
    bool modified; //this node has modification to be applied?
} A1in; //it stores the most recent accessed pages

//this corresponds to the A1out part of the 2Q, which is managed as a FIFO, storing the identifiers of the pages

typedef struct {
    UT_hash_handle hh;

    int page_id; //the key
} A1out; //it stores the ghost pages

static Am *am_part = NULL;
static A1in *a1in_part = NULL;
static A1out *a1out_part = NULL;

/*function for debugging purposes
static void buffer_2q_print() {
    Am *entry_am, *tmp_entry_am;
    A1in *entry_a1in, *tmp_entry_a1in;
    A1out *entry_a1out, *tmp_entry_a1out;
    stringbuffer_t *sb;
    int c = HASH_COUNT(am_part) + HASH_COUNT(a1in_part) + HASH_COUNT(a1out_part);

    sb = stringbuffer_create();

    stringbuffer_append(sb, "Pages in Am: ");

    HASH_ITER(hh, am_part, entry_am, tmp_entry_am) {
        stringbuffer_aprintf(sb, "%d ", entry_am->page_id);
    }

    stringbuffer_append(sb, ". Pages in A1in: ");

    HASH_ITER(hh, a1in_part, entry_a1in, tmp_entry_a1in) {
        stringbuffer_aprintf(sb, "%d ", entry_a1in->page_id);
    }

    stringbuffer_append(sb, ". Pages in A1out: ");

    HASH_ITER(hh, a1out_part, entry_a1out, tmp_entry_a1out) {
        stringbuffer_aprintf(sb, "%d ", entry_a1out->page_id);
    }

    _DEBUGF(NOTICE, "TOTAL OF ELEMENTS: %d. %s", c, stringbuffer_getstring(sb));
    stringbuffer_destroy(sb);
}*/

static void buffer_2q_add_entry(const SpatialIndex *si, int page, uint8_t *buf, bool mod);

void buffer_2q_add_entry(const SpatialIndex *si, int page, uint8_t *buf, bool mod) {
    Am *entry_am, *tmp_entry_am;
    A1in *entry_a1in, *tmp_entry_a1in;
    A1out *entry_a1out, *tmp_entry_a1out;
    int current_size;
    FileSpecification fs;

    if (si->bs->min_capacity < (si->gp->page_size + sizeof (int)) ||
            si->bs->max_capacity < (si->gp->page_size + sizeof (int))) {
        _DEBUGF(WARNING, "The buffer has very low capacity (%zu) and thus, "
                "cannot store any node (size of a node is %d)",
                si->bs->min_capacity, si->gp->page_size);
        if (mod) {
            fs.index_path = si->index_file;
            fs.io_access = si->gp->io_access;
            fs.page_size = si->gp->page_size;

            if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
                disk_write_one_page(&fs, page, buf);
            else if (si->gp->storage_system->type == FLASHDBSIM)
                flashdbsim_write_one_page(si, buf, page);
            else {
                _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);
            }
        }
        return;
    }

    //_DEBUGF(NOTICE, "Putting %d", page);

    HASH_FIND_INT(am_part, &page, entry_am);

    if (entry_am != NULL) {
        // remove it (so the subsequent add will throw it on the front of the list)
        //this happens because ut_hash stores the entries in a doubled linked list
        HASH_DEL(am_part, entry_am);
        HASH_ADD_INT(am_part, page_id, entry_am);
        //it is stored in our buffer
        if (mod) {
            memcpy(entry_am->data, buf, (size_t) si->gp->page_size);
            entry_am->modified = mod;
        }
#ifdef COLLECT_STATISTICAL_DATA
        if (_STORING == 0)
            _sbuffer_page_hit++;
#endif
    } else {
        //we now should check if it is not stored in A1in
        HASH_FIND_INT(a1in_part, &page, entry_a1in);
        if (entry_a1in != NULL) {
            //we only update its content and do not update its position in the list since it is managed as FIFO
            if (mod) {
                memcpy(entry_a1in->data, buf, (size_t) si->gp->page_size);
                entry_a1in->modified = mod;
            }
#ifdef COLLECT_STATISTICAL_DATA
            if (_STORING == 0)
                _sbuffer_page_hit++;
#endif
        } else {
            /*this page is not in Am and A1in, let's check if it on the 'ghost' list, the A1out*/
            //we will only store it on Am if this page is contained on A1out
            Buffer2QSpecification *spec = (Buffer2QSpecification*) si->bs->buf_additional_param;

#ifdef COLLECT_STATISTICAL_DATA
            if (_STORING == 0)
                _sbuffer_page_fault++;
#endif 

            HASH_FIND_INT(a1out_part, &page, entry_a1out);
            if (entry_a1out != NULL) { //ok, we should store it on the Am
                current_size = HASH_COUNT(am_part) * (si->gp->page_size + sizeof (int));
                // prune the cache to MAX_CACHE_SIZE
                if (current_size >= spec->Am_size) {
#ifdef COLLECT_STATISTICAL_DATA
                    struct timespec cpustart;
                    struct timespec cpuend;
                    struct timespec start;
                    struct timespec end;

                    cpustart = get_CPU_time();
                    start = get_current_time();
#endif

                    HASH_ITER(hh, am_part, entry_am, tmp_entry_am) {
                        // prune the first entry (loop is based on insertion order so this deletes the oldest item)
                        HASH_DEL(am_part, entry_am);

                        /* we have to write this page on the disk/flash memory only if this node has modifications*/
                        if (entry_am->modified) {
                            fs.index_path = si->index_file;
                            fs.io_access = si->gp->io_access;
                            fs.page_size = si->gp->page_size;

                            if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
                                disk_write_one_page(&fs, entry_am->page_id, entry_am->data);
                            else if (si->gp->storage_system->type == FLASHDBSIM)
                                flashdbsim_write_one_page(si, entry_am->data, entry_am->page_id);
                            else
                                _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);
                        }

                        if (si->gp->io_access == DIRECT_ACCESS) {
                            free(entry_am->data);
                        } else {
                            lwfree(entry_am->data);
                        }
                        lwfree(entry_am);

                        break;
                    }
#ifdef COLLECT_STATISTICAL_DATA
                    if (_STORING == 0) {
                        cpuend = get_CPU_time();
                        end = get_current_time();

                        _sbuffer_flushing_cpu_time += get_elapsed_time(cpustart, cpuend);
                        _sbuffer_flushing_time += get_elapsed_time(start, end);
                    }
#endif
                }

                entry_am = (Am*) lwalloc(sizeof (Am));
                entry_am->page_id = page;

                if (si->gp->io_access == DIRECT_ACCESS) {
                    //then the memory must be aligned in blocks!
                    if (posix_memalign((void**) &(entry_am->data), si->gp->page_size, si->gp->page_size)) {
                        _DEBUG(ERROR, "Allocation failed at buffer_s2q_add_entry");
                    }
                } else {
                    entry_am->data = (uint8_t*) lwalloc(si->gp->page_size);
                }
                memcpy(entry_am->data, buf, si->gp->page_size);

                entry_am->modified = mod;
                HASH_ADD_INT(am_part, page_id, entry_am);
            } else {
                //this page is not contained in ghost, recent or frequent lists
                //we check if A1in have enough space
                current_size = HASH_COUNT(a1in_part) * (si->gp->page_size + sizeof (int));
                // prune the cache to MAX_CACHE_SIZE
                if (current_size >= spec->A1in_size) {
#ifdef COLLECT_STATISTICAL_DATA
                    struct timespec cpustart;
                    struct timespec cpuend;
                    struct timespec start;
                    struct timespec end;

                    cpustart = get_CPU_time();
                    start = get_current_time();
#endif

                    HASH_ITER(hh, a1in_part, entry_a1in, tmp_entry_a1in) {
                        //in negative case, we remove an entry from A1in, respecting the FIFO, putting it into the 'ghost' list, A1out
                        HASH_DEL(a1in_part, entry_a1in);

                        /* we have to write this page on the disk/flash memory only if this node has modifications*/
                        if (entry_a1in->modified) {
                            fs.index_path = si->index_file;
                            fs.io_access = si->gp->io_access;
                            fs.page_size = si->gp->page_size;

                            if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
                                disk_write_one_page(&fs, entry_a1in->page_id, entry_a1in->data);
                            else if (si->gp->storage_system->type == FLASHDBSIM)
                                flashdbsim_write_one_page(si, entry_a1in->data, entry_a1in->page_id);
                            else
                                _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);
                        }

                        //put the flushed page into a1out
                        current_size = HASH_COUNT(a1out_part);
                        if (current_size >= spec->A1out_size) {
                            HASH_ITER(hh, a1out_part, entry_a1out, tmp_entry_a1out) {
                                HASH_DEL(a1out_part, entry_a1out);
                                lwfree(entry_a1out);
                                break;
                            }
                        }
                        entry_a1out = (A1out*) lwalloc(sizeof (A1out));
                        entry_a1out->page_id = page;
                        HASH_ADD_INT(a1out_part, page_id, entry_a1out);

                        if (si->gp->io_access == DIRECT_ACCESS) {
                            free(entry_a1in->data);
                        } else {
                            lwfree(entry_a1in->data);
                        }
                        lwfree(entry_a1in);

                        break;
                    }

#ifdef COLLECT_STATISTICAL_DATA
                    if (_STORING == 0) {
                        cpuend = get_CPU_time();
                        end = get_current_time();

                        _sbuffer_flushing_cpu_time += get_elapsed_time(cpustart, cpuend);
                        _sbuffer_flushing_time += get_elapsed_time(start, end);
                    }
#endif
                }

                entry_a1in = (A1in*) lwalloc(sizeof (A1in));
                entry_a1in->page_id = page;
                if (si->gp->io_access == DIRECT_ACCESS) {
                    //then the memory must be aligned in blocks!
                    if (posix_memalign((void**) &(entry_a1in->data), si->gp->page_size, si->gp->page_size)) {
                        _DEBUG(ERROR, "Allocation failed at buffer_s2q_add_entry");
                    }
                } else {
                    entry_a1in->data = (uint8_t*) lwalloc(si->gp->page_size);
                }
                memcpy(entry_a1in->data, buf, si->gp->page_size);

                entry_a1in->modified = mod;
                HASH_ADD_INT(a1in_part, page_id, entry_a1in);
            }
        }
    }

    //buffer_2q_print();
}

void buffer_2q_find(const SpatialIndex *si, int page, uint8_t * buf) {
    Am *entry;
    HASH_FIND_INT(am_part, &page, entry);

    //_DEBUGF(NOTICE, "Searching for %d", page);

    if (entry != NULL) {
        // remove it (so the subsequent add will throw it on the front of the list)
        //this happens because ut_hash stores the entries in a doubled linked list
        HASH_DEL(am_part, entry);
        HASH_ADD_INT(am_part, page_id, entry);
        //it is stored in our buffer
        memcpy(buf, entry->data, (size_t) si->gp->page_size);

#ifdef COLLECT_STATISTICAL_DATA
        if (_STORING == 0)
            _sbuffer_page_hit++;
#endif
    } else {
        /*this entry does not exist in Am, we check in A1in*/
        A1in *entry_a1in;
        HASH_FIND_INT(a1in_part, &page, entry_a1in);
        if (entry_a1in != NULL) {
            //it is stored in our buffer, we do not change the position
            memcpy(buf, entry_a1in->data, (size_t) si->gp->page_size);

#ifdef COLLECT_STATISTICAL_DATA
            if (_STORING == 0)
                _sbuffer_page_hit++;
#endif
        } else {
            /* this page is not stored in the buffer, 
             * then we have to get this node from the storage device*/
            FileSpecification fs;
#ifdef COLLECT_STATISTICAL_DATA
            struct timespec cpustart;
            struct timespec cpuend;
            struct timespec start;
            struct timespec end;

            cpustart = get_CPU_time();
            start = get_current_time();
#endif     

            /* we have to read this page from the disk/flash memory*/

            fs.index_path = si->index_file;
            fs.io_access = si->gp->io_access;
            fs.page_size = si->gp->page_size;

            if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
                disk_read_one_page(&fs, page, buf);
            else if (si->gp->storage_system->type == FLASHDBSIM)
                flashdbsim_read_one_page(si, page, buf);
            else
                _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);

            //now we add this page into the buffer
            buffer_2q_add_entry(si, page, buf, false);

#ifdef COLLECT_STATISTICAL_DATA
            if (_STORING == 0) {
                cpuend = get_CPU_time();
                end = get_current_time();

                _sbuffer_find_cpu_time += get_elapsed_time(cpustart, cpuend);
                _sbuffer_find_time += get_elapsed_time(start, end);
            }
#endif
        }
    }

   // buffer_2q_print();
}

void buffer_2q_add(const SpatialIndex *si, int page, uint8_t * buf) {
    buffer_2q_add_entry(si, page, buf, true);
}

void buffer_2q_flush_all(const SpatialIndex * si) {
    Am *entry, *tmp_entry;
    A1in *entry_a1in, *tmp_entry_a1in;
    FileSpecification fs;
    int *pages;
    int count = 0;
    int i = 0;
    uint8_t *loc, *buf;
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    HASH_ITER(hh, am_part, entry, tmp_entry) {
        /* we have to write this page on the disk/flash memory only if this node has modifications*/
        if (entry->modified) {
            count++;
        }
    }

    HASH_ITER(hh, a1in_part, entry_a1in, tmp_entry_a1in) {
        /* we have to write this page on the disk/flash memory only if this node has modifications*/
        if (entry_a1in->modified) {
            count++;
        }
    }

    if (si->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, si->gp->page_size, count * si->gp->page_size)) {
            _DEBUG(ERROR, "Allocation failed at buffer_s2q_add_entry");
        }
    } else {
        buf = (uint8_t*) lwalloc(count * si->gp->page_size);
    }
    pages = (int*) lwalloc(sizeof (int) * count);

    loc = buf;

    HASH_ITER(hh, am_part, entry, tmp_entry) {
        // delete this entry
        HASH_DEL(am_part, entry);

        /* we have to write this page on the disk/flash memory only if this node has modifications*/
        if (entry->modified) {
            pages[i] = entry->page_id;
            memcpy(loc, entry->data, si->gp->page_size);
            loc += si->gp->page_size;
            i++;
        }

        if (si->gp->io_access == DIRECT_ACCESS) {
            free(entry->data);
        } else {
            lwfree(entry->data);
        }
        lwfree(entry);
    }

    HASH_ITER(hh, a1in_part, entry_a1in, tmp_entry_a1in) {
        // delete this entry
        HASH_DEL(a1in_part, entry_a1in);

        /* we have to write this page on the disk/flash memory only if this node has modifications*/
        if (entry->modified) {
            pages[i] = entry_a1in->page_id;
            memcpy(loc, entry_a1in->data, si->gp->page_size);
            loc += si->gp->page_size;
            i++;
        }

        if (si->gp->io_access == DIRECT_ACCESS) {
            free(entry_a1in->data);
        } else {
            lwfree(entry_a1in->data);
        }
        lwfree(entry_a1in);
    }

    if (count > 0) {
        fs.index_path = si->index_file;
        fs.io_access = si->gp->io_access;
        fs.page_size = si->gp->page_size;

        if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
            disk_write(&fs, pages, buf, count);
        else if (si->gp->storage_system->type == FLASHDBSIM)
            flashdbsim_write_pages(si, pages, buf, count);
        else
            _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);
    }
    lwfree(pages);
    if (si->gp->io_access == DIRECT_ACCESS) {
        free(buf);
    } else {
        lwfree(buf);
    }

#ifdef COLLECT_STATISTICAL_DATA
    if (_STORING == 0) {
        cpuend = get_CPU_time();
        end = get_current_time();

        _sbuffer_flushing_cpu_time += get_elapsed_time(cpustart, cpuend);
        _sbuffer_flushing_time += get_elapsed_time(start, end);
    }
#endif
}
