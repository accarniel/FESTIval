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

#include "storage_handler.h"
#include "../buffer/buffer_handler.h" //it also includes the iohandler, and flashdbsimhandler
#include "log_messages.h"

void storage_read_one_page(const SpatialIndex *si, int page, uint8_t *buf, int height) {
    /*this page is stored in the disk?*/
    if (si->bs->buffer_type == BUFFER_NONE) {
        FileSpecification fs;
        fs.index_path = si->index_file;
        fs.io_access = si->gp->io_access;
        fs.page_size = si->gp->page_size;

        if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
            disk_read_one_page(&fs, page, buf);
        else if (si->gp->storage_system->type == FLASHDBSIM) {
            flashdbsim_read_one_page(si, page, buf);
        } else
            _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);
    } else { /*in this case, this page is stored in a buffer scheme*/
        switch (si->bs->buffer_type) {
            case BUFFER_LRU:
            {
                buffer_lru_find(si, page, buf);
                break;
            }
            case BUFFER_HLRU:
            {
                buffer_hlru_find(si, page, buf, height);
                break;
            }
            case BUFFER_S2Q:
            {
                buffer_s2q_find(si, page, buf);
                break;
            }
            case BUFFER_2Q:
            {
                buffer_2q_find(si, page, buf);
                break;
            }
            default:
                _DEBUGF(ERROR, "There is no this buffer scheme: %d ", si->bs->buffer_type);
        }
    }
}

void storage_write_one_page(const SpatialIndex *si, uint8_t *buf, int page, int height) {   
    /*this page is stored in the disk?*/
    if (si->bs->buffer_type == BUFFER_NONE) {
        FileSpecification fs;
        fs.index_path = si->index_file;
        fs.io_access = si->gp->io_access;
        fs.page_size = si->gp->page_size;

        if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
            disk_write_one_page(&fs, page, buf);
        else if (si->gp->storage_system->type == FLASHDBSIM) {
            flashdbsim_write_one_page(si, buf, page);
        } else
            _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);
    } else { /*in this case, this page is stored in a buffer scheme*/
        switch (si->bs->buffer_type) {
            case BUFFER_LRU:
            {
                buffer_lru_add(si, page, buf);
                break;
            }
            case BUFFER_HLRU:
            {
                buffer_hlru_add(si, page, buf, height);
                break;
            }
            case BUFFER_S2Q:
            {
                buffer_s2q_add(si, page, buf);
                break;
            }
            case BUFFER_2Q:
            {
                buffer_2q_add(si, page, buf);
                break;
            }
            default:
                _DEBUGF(ERROR, "There is no this buffer scheme: %d ", si->bs->buffer_type);
        }
    }
}

void storage_read_pages(const SpatialIndex *si, int *pages, uint8_t *buf, int *height, int pagenum) {
    /*this page is stored in the disk?*/
    if (si->bs->buffer_type == BUFFER_NONE) {
        FileSpecification fs;
        fs.index_path = si->index_file;
        fs.io_access = si->gp->io_access;
        fs.page_size = si->gp->page_size;

        if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
            disk_read(&fs, pages, buf, pagenum);
        else if (si->gp->storage_system->type == FLASHDBSIM) {
            flashdbsim_read_pages(si, pages, buf, pagenum);
        } else
            _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);
    } else { /*in this case, this page is stored in a buffer scheme*/
        /*since the available buffers do not support sequential reads, we perform pagenum reads*/
        int i;
        int page_size = si->gp->page_size;
        switch (si->bs->buffer_type) {
            case BUFFER_LRU:
            {
                for (i = 0; i < pagenum; i++) {
                    buffer_lru_find(si, pages[i], buf + i * page_size);
                }
                break;
            }
            case BUFFER_HLRU:
            {
                for (i = 0; i < pagenum; i++) {
                    buffer_hlru_find(si, pages[i], buf + i * page_size, height[i]);
                }
                break;
            }
            case BUFFER_S2Q:
            {
                for (i = 0; i < pagenum; i++) {
                    buffer_s2q_find(si, pages[i], buf + i * page_size);
                }
                break;
            }
            case BUFFER_2Q:
            {
                for (i = 0; i < pagenum; i++) {
                    buffer_2q_find(si, pages[i], buf + i * page_size);
                }
                break;
            }
            default:
                _DEBUGF(ERROR, "There is no this buffer scheme: %d ", si->bs->buffer_type);
        }
    }
}

void storage_write_pages(const SpatialIndex *si, int *pages, uint8_t *buf, int *height, int pagenum) {
    /*this page is stored in the disk?*/
    if (si->bs->buffer_type == BUFFER_NONE) {
        FileSpecification fs;
        fs.index_path = si->index_file;
        fs.io_access = si->gp->io_access;
        fs.page_size = si->gp->page_size;

        if (si->gp->storage_system->type == SSD || si->gp->storage_system->type == HDD)
            disk_write(&fs, pages, buf, pagenum);
        else if (si->gp->storage_system->type == FLASHDBSIM) {
            flashdbsim_write_pages(si, pages, buf, pagenum);
        } else
            _DEBUGF(ERROR, "There is no this storage system: %d ", si->gp->storage_system->type);
    } else { /*in this case, this page is stored in a buffer scheme*/
        /*since the available buffers do not support sequential writes, we perform pagenum writes*/
        int i;
        int page_size = si->gp->page_size;
        switch (si->bs->buffer_type) {
            case BUFFER_LRU:
            {
                for (i = 0; i < pagenum; i++) {
                    buffer_lru_add(si, pages[i], buf + i * page_size);
                }
                break;
            }
            case BUFFER_HLRU:
            {
                for (i = 0; i < pagenum; i++) {
                    buffer_hlru_add(si, pages[i], buf + i * page_size, height[i]);
                }
                break;
            }
            case BUFFER_S2Q:
            {
                for (i = 0; i < pagenum; i++) {
                    buffer_s2q_add(si, pages[i], buf + i * page_size);
                }
                break;
            }
            case BUFFER_2Q:
            {
                for (i = 0; i < pagenum; i++) {
                    buffer_2q_add(si, pages[i], buf + i * page_size);
                }
                break;
            }
            default:
                _DEBUGF(ERROR, "There is no this buffer scheme: %d ", si->bs->buffer_type);
        }
    }
}

void storage_update_tree_height(const SpatialIndex *si, int new_height) {
    if (si->bs->buffer_type == BUFFER_HLRU) {
        //an update is only needed for HLRU
        buffer_hlru_update_tree_height(new_height);
    } //other types of buffers should be included here as elseif cases
}

void storage_flush_all(const SpatialIndex *si) {
    switch (si->bs->buffer_type) {
        case BUFFER_NONE: //there is nothing to do here
            break;
        case BUFFER_LRU:
        {
            buffer_lru_flush_all(si);
            break;
        }
        case BUFFER_HLRU:
        {
            buffer_hlru_flush_all(si);
            break;
        }
        case BUFFER_S2Q:
        {
            buffer_s2q_flush_all(si);
            break;
        }
        case BUFFER_2Q:
        {
            buffer_2q_flush_all(si);
            break;
        }
        default:
            _DEBUGF(ERROR, "There is no this buffer scheme: %d ", si->bs->buffer_type);
    }
}

//flash for flashdbsim to know if it is ready to be used or not
bool is_flashdbsim_initialized = false;

/*this function is for flash simulators that need to be initialized*/
void check_flashsimulator_initialization(const StorageSystem *s) {
    if (!is_flashdbsim_initialized) {
        if (s->type == FLASHDBSIM) {
            FlashDBSim *f = (FlashDBSim*) s->info;
            flashdbsim_initialize(f);
            is_flashdbsim_initialized = true;
        }
    }
}
