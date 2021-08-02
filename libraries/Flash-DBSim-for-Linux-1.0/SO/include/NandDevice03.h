/*
 * Flash-DBSim Storage Simulation Environment
 * NandDevice03.h - Nand device, type 03. See also 'NandDevice03.readme'.
 * Author: Su.Xuan <sdbchina|mail.ustc.edu.cn>
 *
 * Copyright (c) 2009 KDELab@USTC.
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

#ifndef __NAND_DEVICE_03_H_INCLUDED__
#define __NAND_DEVICE_03_H_INCLUDED__

#include "interface.h"
#include "NandDevice02.h"

/* NandDevice03. See also 'NandDevice03.readme'
 */
class NandDevice03 : public NandDevice02
{
protected:
	BYTE * vfSpace;	/* virtual flash storage space in memory */

	/* Constructor & Destructor */
public:
	NandDevice03(void);
	virtual ~NandDevice03(void);

public:
	virtual RV QueryInterface(const IID& /*iid*/, void ** /*ppv*/);

public:
	/* Methods */
	virtual RV Initialize(const VFD_INFO& /*info*/);
	virtual RV Release(void);

	virtual RV EraseBlock(BLOCK_ID /*blockID*/);
	virtual RV ReadPage(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/, BYTE * /*buffer*/, int /*offset*/ = 0, int /*size*/ = 0);
    virtual RV WritePage(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/, const BYTE * /*buffer*/, int /*offset*/ = 0, int /*size*/ = 0);
}; // class NandDevice03

#endif //__NAND_DEVICE_03_H_INCLUDED__
