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
 * File:   efind_buffer_manager.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 14, 2017, 1:07 AM
 */

#ifndef EFIND_BUFFER_H
#define EFIND_BUFFER_H

#include "efind.h"
#include "efind_page_handler.h"

#define eFIND_STATUS_NEW     1
#define eFIND_STATUS_MOD     2
#define eFIND_STATUS_DEL     3

/* FUNCTIONS FOR THE WRITE BUFFER - THESE FUNCTIONS SHOULD BE USED BY THE INDICES! */

/*only create a new rnode in the buffer - this node has no modifications!*/
extern void efind_buf_create_node(const SpatialIndex *base, eFINDSpecification *spec, 
        int new_node_page, int height);

/*put a modification of an existing node (this can be any node, RNode, Hilbert node, and so on)*/
extern void efind_buf_mod_node(const SpatialIndex *base, eFINDSpecification *spec,
        int node_page, void *entry, int height);

/*delete a rnode (which can be stored in the disk or not)*/
extern void efind_buf_del_node(const SpatialIndex *base, eFINDSpecification *spec, 
        int node_page, int height);

/*we retrieve the most recent version of a node by considering possible modification in the buffer
 after the call, we return the most recent version of the request node (rnode_page)
 it returns a void pointer to the node structure that the index is handling
 e.g., an RNode for Rtree indices*/
extern void *efind_buf_retrieve_node(const SpatialIndex *base, const eFINDSpecification *spec, 
        int node_page, int height);
/* it returns the number of elements contained in the write buffer*/
extern unsigned int efind_writebuffer_number_of_elements(void);

/* FUNCTIONS FOR THE READ BUFFER */

/*we always returns a clone of the requested element stored in the buffer*/
extern UIPage *efind_get_node_from_readbuffer(const SpatialIndex *base, const eFINDSpecification *spec,
        int node_page, int height);
/* the boolean parameter means that the element will be stored in the read buffer
we always store a clone of the node element*/
extern void efind_put_node_in_readbuffer(const SpatialIndex *base, const eFINDSpecification *spec,
        UIPage *page, int rrn, int height, bool force);
/* it returns the number of elements contained in the read buffer*/
extern unsigned int efind_readbuffer_number_of_elements(const eFINDSpecification *spec);

/* FUNCTIONS FOR DESTROYING THE BUFFERS */

/*destroy all the variables related to the write buffer and read buffer*/
extern void efind_write_buf_destroy(uint8_t index_type);
extern void efind_read_buf_destroy(const eFINDSpecification *spec, uint8_t index_type);

#endif /* EFIND_BUFFER_H */

