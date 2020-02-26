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
 * File:   efind_read_buffer_policies.h
 * Author: anderson
 *
 * Created on September 4, 2017, 6:18 PM
 */

#ifndef EFIND_READ_BUFFER_POLICIES_H
#define EFIND_READ_BUFFER_POLICIES_H

#include "efind.h" //for basic information
#include "efind_page_handler.h"

/* The general situation to call these functions are the following:
 * 1 - PUT called inside GET, where the page was not modified (since it represents what is stored in the SSD)
 * 2 - UPDATE_ID_NEEDED called during a flushing operation, this indicates that we should update the content of a page if it was stored in the read buffer
 * 3 - PUT called by the temporal control for reads during a flushing operation, 
 *     this indicates that we should update the content of a page if it was stored in the read buffer, or
 *     insert it, otherwise.*/

/***********************************
 * LRU BUFFER
 * *********************************/
extern UIPage *efind_readbuffer_lru_get(const SpatialIndex *base, 
        const eFINDSpecification *spec, int node_page, int height);
extern void efind_readbuffer_lru_put(const SpatialIndex *base, 
        const eFINDSpecification *spec, UIPage *page, int node_page, bool mod);
extern void efind_readbuffer_lru_update_if_needed(const SpatialIndex* base, 
        const eFINDSpecification *spec, int node_page, UIPage *flushed);
extern void efind_readbuffer_lru_destroy(uint8_t index_type);
extern unsigned int efind_readbuffer_lru_number_of_elements(void);

/***********************************
 * Hierarchical LRU BUFFER
 * *********************************/
extern UIPage *efind_readbuffer_hlru_get(const SpatialIndex *base, 
        const eFINDSpecification *spec, int node_page, int height);
extern void efind_readbuffer_hlru_put(const SpatialIndex *base, 
        const eFINDSpecification *spec, UIPage *node, int node_page, int height, bool mod);
extern void efind_readbuffer_hlru_update_if_needed(const SpatialIndex* base, 
        const eFINDSpecification *spec, int node_page, int height, UIPage *flushed);
extern void efind_readbuffer_hlru_set_tree_height(int tree_h);
extern void efind_readbuffer_hlru_destroy(uint8_t index_type);
extern unsigned int efind_readbuffer_hlru_number_of_elements(void);

/***********************************
 * Simplified 2Q BUFFER, USING THE LRU
 * *********************************/
extern UIPage *efind_readbuffer_s2q_get(const SpatialIndex *base, 
        const eFINDSpecification *spec, int node_page, int height);
extern void efind_readbuffer_s2q_put(const SpatialIndex *base, 
        const eFINDSpecification *spec, UIPage *node, int node_page, bool mod);
extern void efind_readbuffer_s2q_update_if_needed(const SpatialIndex* base, 
        const eFINDSpecification *spec, int node_page, UIPage *flushed);
extern void efind_readbuffer_s2q_destroy(uint8_t index_type);
extern unsigned int efind_readbuffer_s2q_number_of_elements(void);

/***********************************
 * Full version of 2Q BUFFER, USING THE LRU
 * *********************************/
extern void efind_readbuffer_2q_setsizes(const eFINDSpecification *spec, int page_size);
extern UIPage *efind_readbuffer_2q_get(const SpatialIndex *base, 
        const eFINDSpecification *spec, int node_page, int height);
extern void efind_readbuffer_2q_put(const SpatialIndex *base, 
        const eFINDSpecification *spec, UIPage *node, int node_page, bool mod);
extern void efind_readbuffer_2q_update_if_needed(const SpatialIndex* base, int node_page, UIPage *flushed);
extern void efind_readbuffer_2q_destroy(uint8_t index_type);
extern unsigned int efind_readbuffer_2q_number_of_elements(void);

#endif /* EFIND_READ_BUFFER_POLICIES_H */

