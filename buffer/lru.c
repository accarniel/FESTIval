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

/*this file is responsible to implement the LRU replacement cache */
#include "buffer_handler.h"
#include "../libraries/uthash/uthash.h"
#include "../main/log_messages.h"

#include "../main/statistical_processing.h"

#include <stringbuffer.h> /* for printing */

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)

#undef uthash_fatal
#define uthash_fatal(msg) _DEBUG(ERROR, msg)

typedef struct LRU {
    UT_hash_handle hh;

    int page_id; //the key
    uint8_t *data; //the serialized node
    bool modified; //this node has modification to be applied?
} LRU;

/*
 * Some considerations about the size of the buffer:
 * 1 - it only considers the size of the node and its id (that is: page_size + sizeof(int))
 * 2 - it does not consider the overhead size introduced by the UT_hash_handle!
 */

static LRU *lru = NULL;

static void buffer_lru_add_entry(const SpatialIndex *si, int page, uint8_t *buf, bool mod);

/*function for debugging purposes
static void buffer_lru_print() {
    LRU *entry, *tmp_entry;
    stringbuffer_t *sb;
    int c = HASH_COUNT(lru);

    sb = stringbuffer_create();

    stringbuffer_append(sb, "Pages in the buffer: ");

    HASH_ITER(hh, lru, entry, tmp_entry) {
        stringbuffer_aprintf(sb, "%d ", entry->page_id);
    }

    _DEBUGF(NOTICE, "TOTAL OF ELEMENTS: %d. %s", c, stringbuffer_getstring(sb));
    stringbuffer_destroy(sb);
}*/

void buffer_lru_add_entry(const SpatialIndex *si, int page, uint8_t *buf, bool mod) {
    LRU *entry, *tmp_entry;
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

    HASH_FIND_INT(lru, &page, entry);
    //we have to update this value
    if (entry != NULL) {
        // remove it (so the subsequent add will throw it on the front of the list)
        //this happens because ut_hash stores the entries in a doubled linked list
        HASH_DEL(lru, entry);
        HASH_ADD_INT(lru, page_id, entry);

        if (mod) {
            entry->modified = mod;
            //it is stored in our buffer
            memcpy(entry->data, buf, (size_t) si->gp->page_size);
        }

#ifdef COLLECT_STATISTICAL_DATA
        if (_STORING == 0)
            _sbuffer_page_hit++;
#endif
    } else {
#ifdef COLLECT_STATISTICAL_DATA
        if (_STORING == 0)
            _sbuffer_page_fault++;
#endif

        //check if we have enough space
        current_size = HASH_COUNT(lru) * (si->gp->page_size + sizeof (int));
        // prune the cache to MAX_CACHE_SIZE
        if (current_size >= si->bs->max_capacity) {
#ifdef COLLECT_STATISTICAL_DATA
            struct timespec cpustart;
            struct timespec cpuend;
            struct timespec start;
            struct timespec end;

            cpustart = get_CPU_time();
            start = get_current_time();
#endif

            HASH_ITER(hh, lru, entry, tmp_entry) {
                // prune the first entry (loop is based on insertion order so this deletes the oldest item)
                HASH_DEL(lru, entry);

                /* we have to write this page on the disk/flash memory only if this node has modifications*/
                if (entry->modified) {
                    fs.index_path = si->index_file;
                    fs.io_access = si->gp->io_access;
                    fs.page_size = si->gp->page_size;

                    if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
                        disk_write_one_page(&fs, entry->page_id, entry->data);
                    else if (si->gp->storage_system->type == FLASHDBSIM)
                        flashdbsim_write_one_page(si, entry->data, entry->page_id);
                    else
                        _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);
                }

                if (si->gp->io_access == DIRECT_ACCESS) {
                    free(entry->data);
                } else {
                    lwfree(entry->data);
                }
                lwfree(entry);

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

        entry = (LRU*) lwalloc(sizeof (LRU));
        entry->page_id = page;

        if (si->gp->io_access == DIRECT_ACCESS) {
            //then the memory must be aligned in blocks!
            if (posix_memalign((void**) &(entry->data), si->gp->page_size, si->gp->page_size)) {
                _DEBUG(ERROR, "Allocation failed at buffer_hlru_add_entry");
            }
        } else {
            entry->data = (uint8_t*) lwalloc(si->gp->page_size);
        }
        memcpy(entry->data, buf, si->gp->page_size);

        entry->modified = mod;
        HASH_ADD_INT(lru, page_id, entry);
    }

    //buffer_lru_print();

}

void buffer_lru_find(const SpatialIndex *si, int page, uint8_t *buf) {
    LRU *entry;
    HASH_FIND_INT(lru, &page, entry);

    //_DEBUGF(NOTICE, "Searching for %d", page);

    if (entry != NULL) {
        // remove it (so the subsequent add will throw it on the front of the list)
        //this happens because ut_hash stores the entries in a doubled linked list
        HASH_DEL(lru, entry);
        HASH_ADD_INT(lru, page_id, entry);
        //it is stored in our buffer
        memcpy(buf, entry->data, (size_t) si->gp->page_size);

#ifdef COLLECT_STATISTICAL_DATA
        if (_STORING == 0)
            _sbuffer_page_hit++;
#endif
    } else {
        //this entry does not exist in our LRU buffer
        //then we have to get this node from the disk/flash
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
        buffer_lru_add_entry(si, page, buf, false);

#ifdef COLLECT_STATISTICAL_DATA
        if (_STORING == 0) {
            cpuend = get_CPU_time();
            end = get_current_time();

            _sbuffer_find_cpu_time += get_elapsed_time(cpustart, cpuend);
            _sbuffer_find_time += get_elapsed_time(start, end);
        }
#endif
    }

    //buffer_lru_print();
}

void buffer_lru_add(const SpatialIndex *si, int page, uint8_t *buf) {
    buffer_lru_add_entry(si, page, buf, true);
}

void buffer_lru_flush_all(const SpatialIndex *si) {
    LRU *entry, *tmp_entry;
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

    HASH_ITER(hh, lru, entry, tmp_entry) {
        /* we have to write this page on the disk/flash memory only if this node has modifications*/
        if (entry->modified) {
            count++;
        }
    }

    if (si->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, si->gp->page_size, count * si->gp->page_size)) {
            _DEBUG(ERROR, "Allocation failed at buffer_hlru_add_entry");
        }
    } else {
        buf = (uint8_t*) lwalloc(count * si->gp->page_size);
    }
    pages = (int*) lwalloc(sizeof (int) * count);

    loc = buf;

    HASH_ITER(hh, lru, entry, tmp_entry) {
        // delete this entry
        HASH_DEL(lru, entry);

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
