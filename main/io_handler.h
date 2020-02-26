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
 * File:   io_handler.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 24, 2016, 6:28 PM
 */

#ifndef IO_HANDLER_H
#define IO_HANDLER_H

#include <stdint.h>

/*definition of the type of access of a file */
#define NORMAL_ACCESS		1
#define DIRECT_ACCESS		2

typedef int IDX_FILE;

typedef struct {
    char *index_path;
    int page_size;
    uint8_t io_access;
} FileSpecification;

/* perform the write and read operations in ONE page/node 
 this is for that implementations based on HDD
 */
extern void disk_read_one_page(const FileSpecification *fs, int page, uint8_t *buf);
extern void disk_write_one_page(const FileSpecification *fs, int page, uint8_t *buf);

/* perform the write and read operations in an array of pages/nodes (for flash-aware indices)
for pages that were allocated sequentially, this function writes it also sequentially
for instance, pages 1, 2, and 3 will be sequentially written in only one raw_read operation
 * 
 * buf is also an array of BYTES separated by page_size 
 * (e.g., buf + pos*page_size extracts the page/node of position pos)
 */
extern void disk_write(const FileSpecification *fs, int *pages, uint8_t *buf, int pagenum);
extern void disk_read(const FileSpecification *fs, int *pages, uint8_t *buf, int pagenum);


#endif /* _IO_HANDLER_H */

