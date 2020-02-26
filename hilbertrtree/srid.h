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
 * File:   srid.h
 * Author: anderson
 *
 * Created on November 22, 2017, 5:42 PM
 */

#ifndef SRID_H
#define SRID_H

#define SRID_4326_MinX -180.0
#define SRID_4326_MaxX  180.0 
#define SRID_4326_MinY -90.0 
#define SRID_4326_MaxY 	90.0

#define SRID_3857_MinX -20037508.3427892
#define SRID_3857_MaxX  20037508.3427892
#define SRID_3857_MinY -20037508.3430388 
#define SRID_3857_MaxY 	20037508.3430388

#define SRID_2029_MinX  309888.257034626
#define SRID_2029_MaxX  690111.742965374
#define SRID_2029_MinY  4617073.21036364
#define SRID_2029_MaxY 	6139847.98761672

/* to define the max and min values for a new SRID, you should follow the same pattern above
 that is, SRID_{YOUR SRID NUMBER}_[MaxX, MaxY, MinX, MinY]*/

#endif /* SRID_H */

