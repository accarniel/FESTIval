/*
 * Flash-DBSim Storage Simulation Environment
 * NandDevice02.h - Nand device, type 02. See also 'NandDevice02.readme'.
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

#ifndef __NAND_DEVICE_02_H_INCLUDED__
#define __NAND_DEVICE_02_H_INCLUDED__

#include "interface.h"
#include "NandDevice01.h"

/* NandDevice02. See also 'NandDevice02.readme'
 */
class NandDevice02 : public NandDevice01, public IVFD_LATENCY
{
	/* Constructor & Destructor */
public:
	NandDevice02(void);
	virtual ~NandDevice02(void);

public:
	virtual RV QueryInterface(const IID& /*iid*/, void ** /*ppv*/);

public:
	/* Methods */
	virtual RV Initialize(const VFD_INFO& /*info*/);
	virtual RV Release(void);

	virtual RV EraseBlock(BLOCK_ID /*blockID*/);
	virtual RV ReadPage(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/, BYTE * /*buffer*/, int /*offset*/ = 0, int /*size*/ = 0);
    virtual RV WritePage(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/, const BYTE * /*buffer*/, int /*offset*/ = 0, int /*size*/ = 0);
}; // class NandDevice02

#endif //__NAND_DEVICE_02_H_INCLUDED__
