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
 * File:   storage_handler.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 5, 2017, 9:11 PM
 */

#ifndef STORAGE_HANDLER_H
#define STORAGE_HANDLER_H

#include "spatial_index.h"

/*these functions decide where the data of a node will be stored.
 It can be stored in a buffer in the main memory, such as a LRU buffer
 or
 It can be stored directly into the disk/flash memory.
 
 This also happens for read operations.*/
extern void storage_read_one_page(const SpatialIndex *si, int page, uint8_t *buf, int height); 
extern void storage_write_one_page(const SpatialIndex *si, uint8_t *buf, int page, int height);

/*this is for the handling of several pages together, which commonly will be sequential pages
 when the current index is using traditional buffers:
 * when pagenum is greater than 0, it will perform pagenum times teh corresponding function.
 For instance, storage_write_pages will call pagenum times the storage_write_one_page 
 * since there is a sequential write for the current implemented buffers (e.g., LRU)
 * however, when the current index is NOT using traditional buffer:
 * a sequential write is directly done in the disk/flash memory */
extern void storage_read_pages(const SpatialIndex *si, int *pages, uint8_t *buf, int *height, int pagenum);
extern void storage_write_pages(const SpatialIndex *si, int *pages, uint8_t *buf, int *height, int pagenum);

/* this function is needed because of the HLRU */
extern void storage_update_tree_height(const SpatialIndex *si, int new_height);

/* if a buffer is used by this index, them we apply all the modifications contained in such buffer*/
extern void storage_flush_all(const SpatialIndex *si);

extern bool is_flashdbsim_initialized;

extern void check_flashsimulator_initialization(const StorageSystem *s);


#endif /* _STORAGE_HANDLER_H */

