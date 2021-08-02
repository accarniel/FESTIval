/*
 * Flash-DBSim Storage Simulation Environment
 * FlashDBSim.h - Public interfaces(functions) of Flash-DBSim.
 * Author: Su.Xuan <sdbchina|mail.ustc.edu.cn>
 *
 * Copyright (c) 2008-2009 KDELab@USTC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __FLASH_DBSIM_H_INCLUDED__
#define __FLASH_DBSIM_H_INCLUDED__

#include "interface.h"

/* Public interfaces of Flash-DBSim System */
typedef class FlashDBSim {
protected:
	static IFTL *	ftl;

public:
	static const IFTL * GetFTLModule(void) { return FlashDBSim::ftl; }

	/* Constructor */
protected:
	FlashDBSim(void) {}

public:
	static RV Initialize(const VFD_INFO& /*vfdInfo*/, const FTL_INFO& /*ftlInfo*/);	/* Initialization */
	static RV Release(void);	/* Release Flash-DBSim system */

	static int AllocPage(int /*count*/, LBA * /*lbas*/);
	static RV ReleasePage(LBA /*lba*/);

	static RV ReadPage(LBA /*lba*/, BYTE * /*buffer*/, int /*offset*/ = 0, size_t /*size*/ = 0);
	static RV WritePage(LBA /*lba*/, const BYTE * /*buffer*/, int /*offset*/ = 0, size_t /*size*/ = 0);
} FlashDBSim; // class FlashDBSim

#endif //__FLASH_DBSIM_H_INCLUDED__
