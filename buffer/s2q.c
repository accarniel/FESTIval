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
 * This file implements the simplified version of the 2Q cache management proposed in
 * 
 * Reference: JOHNSON, T.; SHASHA, D. 2Q: A Low Overhead High Performance 
 * Buffer Management Replacement Algorithm. In Proceedings of the 
 * 20th International Conference on Very Large Data Bases (VLDB '94), p. 439-450, 1994.
 * 
 * this uses the implementation based on tags
 * 
 * this implementation is also based on :
 * 
 * LERSCH, L.; OUKID, I.; SCHRETER, I.; LEHNER, W. Rethinking DRAM Caching for LSMs in an NVRAM Environment.
 * In Proceedings of the Advances in Databases and Information Systems (ADBIS'17), p. 326-340, 2017.
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


//this corresponds to the Am part of the S2Q, which is managed as a LRU cache

typedef struct {
    UT_hash_handle hh;

    int page_id; //the key
    uint8_t *data; //the serialized node
    bool modified; //this node has modification to be applied?
} Am; //it stores the most frequent accessed pages

//this corresponds to the A1 part of the S2Q, which is managed as a FIFO

typedef struct {
    UT_hash_handle hh;

    int page_id; //the key    
} A1; //it stores the most recent accessed pages

static Am *am_part = NULL;
static A1 *a1_part = NULL;

/*function for debugging purposes
static void buffer_s2q_print() {
    Am *entry_am, *tmp_entry_am;
    A1 *entry_a1, *tmp_entry_a1;
    stringbuffer_t *sb;
    int c = HASH_COUNT(am_part) + HASH_COUNT(a1_part);

    sb = stringbuffer_create();

    stringbuffer_append(sb, "Pages in Am: ");

    HASH_ITER(hh, am_part, entry_am, tmp_entry_am) {
        stringbuffer_aprintf(sb, "%d ", entry_am->page_id);
    }

    stringbuffer_append(sb, ". Pages in A1: ");

    HASH_ITER(hh, a1_part, entry_a1, tmp_entry_a1) {
        stringbuffer_aprintf(sb, "%d ", entry_a1->page_id);
    }

    _DEBUGF(NOTICE, "TOTAL OF ELEMENTS: %d. %s", c, stringbuffer_getstring(sb));
    stringbuffer_destroy(sb);
    
    if(HASH_COUNT(a1_part) == 0 && HASH_COUNT(am_part) == 5)
        _DEBUG(ERROR, "CHECAR");
}*/

static void buffer_s2q_add_entry(const SpatialIndex *si, int page, uint8_t *buf, bool mod);

void buffer_s2q_add_entry(const SpatialIndex *si, int page, uint8_t *buf, bool mod) {
    Am *entry_am, *tmp_entry_am;
    A1 *entry_a1, *tmp_entry_a1;
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

   // _DEBUGF(NOTICE, "Putting %d", page);

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
        //we will only store it on Am if this page is contained on A1
        BufferS2QSpecification *spec = (BufferS2QSpecification*) si->bs->buf_additional_param;

#ifdef COLLECT_STATISTICAL_DATA
        if (_STORING == 0)
            _sbuffer_page_fault++;
#endif 

        HASH_FIND_INT(a1_part, &page, entry_a1);
        if (entry_a1 != NULL) { //ok, we should store it on the Am and remove from A1           
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

            //remove from A1
            HASH_DEL(a1_part, entry_a1);
            lwfree(entry_a1);
        } else {
            //we check if A1 have enough space
            current_size = HASH_COUNT(a1_part);
            // prune the cache to MAX_CACHE_SIZE
            if (current_size >= spec->A1_size) {

                HASH_ITER(hh, a1_part, entry_a1, tmp_entry_a1) {
                    //in negative case, we remove an entry from A1in, respecting the FIFO
                    HASH_DEL(a1_part, entry_a1);
                    lwfree(entry_a1);
                    break;
                }
            }

            entry_a1 = (A1*) lwalloc(sizeof (A1));
            entry_a1->page_id = page;
            HASH_ADD_INT(a1_part, page_id, entry_a1);

            //after add in A1 part, we should write the page in the storage device if needed
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
        }
    }

    //buffer_s2q_print();
}

void buffer_s2q_find(const SpatialIndex *si, int page, uint8_t * buf) {
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
        buffer_s2q_add_entry(si, page, buf, false);

#ifdef COLLECT_STATISTICAL_DATA
        if (_STORING == 0) {
            cpuend = get_CPU_time();
            end = get_current_time();

            _sbuffer_find_cpu_time += get_elapsed_time(cpustart, cpuend);
            _sbuffer_find_time += get_elapsed_time(start, end);
        }
#endif
    }

   // buffer_s2q_print();
}

void buffer_s2q_add(const SpatialIndex *si, int page, uint8_t * buf) {
    buffer_s2q_add_entry(si, page, buf, true);
}

void buffer_s2q_flush_all(const SpatialIndex * si) {
    Am *entry, *tmp_entry;
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
