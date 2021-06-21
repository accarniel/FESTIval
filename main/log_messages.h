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
 * File:   log_messages.h
 * Author: Anderson Chaves Carniel
 *
 * Created on February 24, 2016, 8:47 PM
 */

#ifndef LOG_MESSAGES_H
#define LOG_MESSAGES_H

#include <postgres.h>

#define DEBUG_LEVEL 1

/* variable level can be: */
/* INFO     Messages specifically requested by user (eg VACUUM VERBOSE output); always sent to
 * client regardless of client_min_messages,
 * but by default not sent to server log. */

/* NOTICE      Helpful messages to users about query
 * operation; sent to client and not to server
 * log by default. */

/* WARNING     Warnings.  NOTICE is for expected messages
 * like implicit sequence creation by SERIAL.
 * WARNING is for unexpected messages. */

/* ERROR       user error - abort transaction; return to
 * known state */

#if DEBUG_LEVEL > 0

/* Display a simple message by using the ereport from postgres library */
/* from PgSQL utils/elog.h */
#define _DEBUG(level, msg) \
        do { \
                ereport((level), (errmsg_internal("[%s:%s:%d] " msg, __FILE__, __func__, __LINE__))); \
        } while (0);
/* Display a simple message by using the ereport from postgres library (formated version) */
#define _DEBUGF(level, msg, ...) \
        do { \
                ereport((level), (errmsg_internal("[%s:%s:%d] " msg, __FILE__, __func__, __LINE__, __VA_ARGS__))); \
        } while (0);

#else

/* Empty prototype that can be optimized away by the compiler for non-debug builds */
#define _DEBUG(level, msg) \
        ((void) 0)

/* Empty prototype that can be optimized away by the compiler for non-debug builds */
#define _DEBUGF(level, msg, ...) \
        ((void) 0)

#endif

#endif /* _LOG_MESSAGES_H */

