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
 * File:   flashdbsim.h
 * Author: Anderson
 *
 * Created on May 2017
 */

#ifndef FLASHDBSIM_H
#define FLASHDBSIM_H

#include "../main/spatial_index.h"

/* needed functions to initialize and release the FlashDBSim simulator, respectively
 * the initialize function should be called one time in the application
 * in addition, it created a simulator according to the specifications from the user
 * since the release function kill the current FlashDBSim simulator, it doesn't need any parameter
 */
extern void flashdbsim_initialize(const FlashDBSim *si);
extern void flashdbsim_release(void);

/* It reads and writes one page of the index. That is, it manages the index pages
 * which are stored in flash pages of the FlashDBSim.
 * There are some limitations: we are not able to read and write a sequence of pages, like in the SSD and HDD storage device
 * as consequence, sequential write/read simple do not exist. *
 */
extern void flashdbsim_read_one_page(const SpatialIndex *si, int idx_page, uint8_t *buf);
extern void flashdbsim_write_one_page(const SpatialIndex *si, uint8_t *buf, int idx_page);

/*since we cannot write/read pages in batch, we simple call the aforementioned functions pagenum times*/
extern void flashdbsim_read_pages(const SpatialIndex *si, int *idx_pages, uint8_t *buf, int pagenum);
extern void flashdbsim_write_pages(const SpatialIndex *si, int *idx_pages, uint8_t *buf, int pagenum);


#endif /* FLASHDBSIM_H */

