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
 * File:   lru.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 5, 2017, 10:29 PM
 */

#ifndef BUFFER_HANDLER_H
#define BUFFER_HANDLER_H

#include "../main/spatial_index.h"
#include "../main/io_handler.h"
#include "../flashdbsim/flashdbsim.h"

/***********************************
 * LRU BUFFER
 * *********************************/
extern void buffer_lru_find(const SpatialIndex *si, int page, uint8_t *buf);
extern void buffer_lru_add(const SpatialIndex *si, int page, uint8_t *buf);
extern void buffer_lru_flush_all(const SpatialIndex *si);

/***********************************
 * Hierarchical LRU BUFFER
 * *********************************/
extern void buffer_hlru_find(const SpatialIndex *si, int page, uint8_t *buf, int height);
extern void buffer_hlru_add(const SpatialIndex *si, int page, uint8_t *buf, int height);
extern void buffer_hlru_flush_all(const SpatialIndex *si);
extern void buffer_hlru_update_tree_height(int new_height);



/***********************************
 * The following buffers are variants of the 2Q buffer proposed in
 * 
 * JOHNSON, T.; SHASHA, D. 2Q: A Low Overhead High Performance 
 * Buffer Management Replacement Algorithm. In Proceedings of the 
 * 20th International Conference on Very Large Data Bases (VLDB '94), p. 439-450, 1994.
 * 
 * The simplified 2q version is also based on:
 * 
 * LERSCH, L.; OUKID, I.; SCHRETER, I.; LEHNER, W. Rethinking DRAM Caching for LSMs in an NVRAM Environment.
 * In Proceedings of the Advances in Databases and Information Systems (ADBIS'17), p. 326-340, 2017.
 * 
 * *********************************/

/***********************************
 * Simplified 2Q BUFFER
 * *********************************/
extern void buffer_s2q_find(const SpatialIndex *si, int page, uint8_t *buf);
extern void buffer_s2q_add(const SpatialIndex *si, int page, uint8_t *buf);
extern void buffer_s2q_flush_all(const SpatialIndex *si);

/***********************************
 * Full version 2Q BUFFER
 * *********************************/
extern void buffer_2q_find(const SpatialIndex *si, int page, uint8_t *buf);
extern void buffer_2q_add(const SpatialIndex *si, int page, uint8_t *buf);
extern void buffer_2q_flush_all(const SpatialIndex *si);


#endif /* BUFFER_HANDLER_H */

