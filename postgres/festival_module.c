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

/************************
*
* Init module. It also start the PostGIS module.
*
*************************/

#include "lwgeom_pg.h" //from libpgcommon
#include "postgres.h"
#include "fmgr.h"


/*
 * This is required for builds against pgsql
 */

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif


/*
 * Module load callback
 */
void _PG_init(void);
void
_PG_init(void)
{
    /* install PostgreSQL handlers */
    pg_install_lwgeom_handlers();

    /* initialize geometry backend */
    //lwgeom_init_backend();
}

/*
 * Module unload callback
 */
void _PG_fini(void);
void
_PG_fini(void)
{
  
}
