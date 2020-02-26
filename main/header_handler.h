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
 * File:   header_handler.h
 * Author: Anderson Chaves Carniel
 *
 * Created on March 18, 2016, 9:55 AM
 * Updated on October 16, 2016, 05:25 PM
 */

#ifndef HEADER_HANDLER_H
#define HEADER_HANDLER_H

#include "spatial_index.h"

/* write the index specification in an auxiliary file
 this function is a writer for the spatial indices supported by FESTIval*/
extern void festival_header_writer(const char *idx_spc_path, uint8_t type, SpatialIndex *si);

/* read the index specification and return it. It also reads the root node!!
 This means that it gets the spatial index right to be used!
 The returning SpatialIndex should be only freed when festival_header_writer is called!*/
extern SpatialIndex *festival_get_spatialindex(const char *idx_spc_path);

/*auxiliary functions in order to get the index type (for FAI operations)*/
extern uint8_t hh_get_index_type(const char* path);

#endif /* _HEADER_HANDLER_H */

