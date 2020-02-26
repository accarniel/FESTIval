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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for O_DIRECT */
#endif


#include "io_handler.h"
#include "log_messages.h" /* for log messages */
#include <stdlib.h>  /* for qsort, malloc(3c) */
#include <sys/types.h>  /* required by open() */
#include <unistd.h>     /* open(), write() */
#include <fcntl.h>      /* open() and fcntl() */

#include "statistical_processing.h" /* to collect statistical data */

/* open and close the file */
static IDX_FILE disk_open(const FileSpecification *fs);
static void disk_close(IDX_FILE f);

/* perform the read and write operation directly in a file */
static void raw_read(IDX_FILE f, int page_size, int page_num, uint8_t *buf, int bufsize);
static void raw_write(IDX_FILE f, int page_size, int page_num, uint8_t *buf, int bufsize);

IDX_FILE disk_open(const FileSpecification *fs) {
    int flag;
    IDX_FILE ret;
    if (fs->io_access == NORMAL_ACCESS)
        flag = O_CREAT | O_RDWR;
    else if (fs->io_access == DIRECT_ACCESS) {
        flag = O_CREAT | O_RDWR | O_DIRECT;
    } else {
        _DEBUGF(WARNING, "Unknown access %d to open disk", fs->io_access);
        flag = O_CREAT | O_RDWR; //default access
    }
    if ((ret = open(fs->index_path, flag, S_IRUSR | S_IWUSR)) < 0) {
        _DEBUGF(ERROR, "It was impossible to open the \'%s\'. "
                "It used the following access - %d", fs->index_path, fs->io_access);
    }
    return ret;
}

void disk_close(IDX_FILE f) {
    if (close(f) == -1) {
        _DEBUG(ERROR, "It was impossible to close the file");
    }
}

void raw_read(IDX_FILE f, int page_size, int page_num, uint8_t *buf, int bufsize) {
    int real_size;
    if (lseek(f, page_num * page_size, SEEK_SET) < 0) {
        _DEBUG(ERROR, "Error in lseek in raw_read");
    }

    if ((real_size = read(f, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Sizes do not match in raw_read -> %d - %d -> page number %d", bufsize, real_size, page_num);
    }
}

void raw_write(IDX_FILE f, int page_size, int page_num, uint8_t *buf, int bufsize) {
    int real_size;
    if (lseek(f, page_num * page_size, SEEK_SET) < 0) {
        _DEBUG(ERROR, "Error in lseek in raw_write");
    }

    if ((real_size = write(f, buf, bufsize)) != bufsize) {
        _DEBUGF(ERROR, "Sizes do not match in raw_write -> %d - %d", bufsize, real_size);
    }
}

void disk_write_one_page(const FileSpecification *fs, int page, uint8_t *buf) {
    /* open the file by using the index specification access_io */
    IDX_FILE f = disk_open(fs);
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();

    /* compute the following statistical data: 
     * increment write_time and write_num */
    if (_STORING == 0)
        _write_num++;
#endif

    raw_write(f, fs->page_size, page, buf, fs->page_size);

    /* close the index file */
    disk_close(f);
#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    if (_STORING == 0) {
        _write_cpu_time += get_elapsed_time(cpustart, cpuend);
        _write_time += get_elapsed_time(start, end);

        if (_COLLECT_READ_WRITE_ORDER == 1) {
            append_rw_order(page, WRITE_REQUEST, get_current_time_in_seconds());
        }
    }
#endif
}

void disk_read_one_page(const FileSpecification *fs, int page, uint8_t *buf) {
    /* open the file by using the index specification access_io */
    IDX_FILE f = disk_open(fs);
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
    /* compute the following statistical data: 
     * increment read_time and read_num */
    if (_STORING == 0)
        _read_num++;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    raw_read(f, fs->page_size, page, buf, fs->page_size);

    /* close the index file */
    disk_close(f);
#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    if (_STORING == 0) {
        _read_cpu_time += get_elapsed_time(cpustart, cpuend);
        _read_time += get_elapsed_time(start, end);

        if (_COLLECT_READ_WRITE_ORDER == 1) {
            append_rw_order(page, READ_REQUEST, get_current_time_in_seconds());
        }
    }
#endif  
}

void disk_read(const FileSpecification *fs, int *pages, uint8_t *buf, int pagenum) {
    int pos;
    int tempp;
    int sum;
    IDX_FILE f;
    int i;
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
    int aux;
    double time;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    /* open the file by using the index specification access_io */
    f = disk_open(fs);

    for (i = 0; i < pagenum;) {
        if (i == 0) {
            tempp = pages[i];
            i++;
            pos = 0;
            sum = 1;
        } else {
            if (pages[i] - tempp == sum) {
                i++;
                sum++;
            } else {
                raw_read(f, fs->page_size, tempp, buf + pos * fs->page_size, sum * fs->page_size);
#ifdef COLLECT_STATISTICAL_DATA
                if (_STORING == 0) {
                    _read_num++;
                    if (_COLLECT_READ_WRITE_ORDER == 1) {
                        time = get_current_time_in_seconds();
                        for (aux = 0; aux < sum; aux++)
                            append_rw_order(tempp + aux, READ_REQUEST, time);
                    }
                }
#endif
                pos = i;
                tempp = pages[i];
                i++;
                sum = 1;
            }
        }
        if (i == pagenum) {
            raw_read(f, fs->page_size, tempp, buf + pos * fs->page_size, sum * fs->page_size);
#ifdef COLLECT_STATISTICAL_DATA
            if (_STORING == 0) {
                _read_num++;
                if (_COLLECT_READ_WRITE_ORDER == 1) {
                    time = get_current_time_in_seconds();
                    for (aux = 0; aux < sum; aux++)
                        append_rw_order(tempp + aux, READ_REQUEST, time);
                }
            }
#endif
        }
    }

    /* close the index file */
    disk_close(f);
#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();
    if (_STORING == 0) {

        _read_cpu_time += get_elapsed_time(cpustart, cpuend);
        _read_time += get_elapsed_time(start, end);
    }
#endif  
}

void disk_write(const FileSpecification *fs, int *pages, uint8_t *buf, int pagenum) {
    int pos;
    int tempp;
    int sum;
    IDX_FILE f;
    int i;
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;
    int aux;
    double time;

    cpustart = get_CPU_time();
    start = get_current_time();
#endif

    /* open the file by using the index specification io_access */
    f = disk_open(fs);

    for (i = 0; i < pagenum;) {
        if (i == 0) {
            tempp = pages[i];
            i++;
            pos = 0;
            sum = 1;
        } else {
            if (pages[i] - tempp == sum) {
                i++;
                sum++;
            } else {
                raw_write(f, fs->page_size, tempp, buf + pos * fs->page_size, sum * fs->page_size);

#ifdef COLLECT_STATISTICAL_DATA
                if (_STORING == 0) {
                    _write_num++;
                    if (_COLLECT_READ_WRITE_ORDER == 1) {
                        time = get_current_time_in_seconds();
                        for (aux = 0; aux < sum; aux++)
                            append_rw_order(tempp + aux, WRITE_REQUEST, time);
                    }
                }
#endif

                pos = i;
                tempp = pages[i];
                i++;
                sum = 1;
            }
        }
        if (i == pagenum) {
            raw_write(f, fs->page_size, tempp, buf + pos * fs->page_size, sum * fs->page_size);
#ifdef COLLECT_STATISTICAL_DATA
            if (_STORING == 0) {
                _write_num++;
                if (_COLLECT_READ_WRITE_ORDER == 1) {
                    time = get_current_time_in_seconds();
                    for (aux = 0; aux < sum; aux++)
                        append_rw_order(tempp + aux, WRITE_REQUEST, time);
                }
            }
#endif
        }
    }

    /* close the index file */
    disk_close(f);
#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    if (_STORING == 0) {
        _write_cpu_time += get_elapsed_time(cpustart, cpuend);
        _write_time += get_elapsed_time(start, end);
    }
#endif
}

