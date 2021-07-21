/*
 * Flash-DBSim Storage Simulation Environment
 * NandDevice04.h - Nand device, type 04. See also 'NandDevice04.readme'.
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

#ifndef __NAND_DEVICE_04_H_INCLUDED__
#define __NAND_DEVICE_04_H_INCLUDED__

#include <fstream>

using namespace std;

#include "interface.h"
#include "NandDevice02.h"

/* NandDevice04. See also 'NandDevice04.readme'
 */
class NandDevice04 : public NandDevice02
{
protected:
	fstream vfImage;	/* virtual flash storage space in disk image */

	/* Constructor & Destructor */
public:
	NandDevice04(void);
	virtual ~NandDevice04(void);

public:
	virtual RV QueryInterface(const IID& /*iid*/, void ** /*ppv*/);

public:
	/* Methods */
	virtual RV Initialize(const VFD_INFO& /*info*/);
	virtual RV Release(void);

	virtual RV EraseBlock(BLOCK_ID /*blockID*/);
	virtual RV ReadPage(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/, BYTE * /*buffer*/, int /*offset*/ = 0, int /*size*/ = 0);
    virtual RV WritePage(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/, const BYTE * /*buffer*/, int /*offset*/ = 0, int /*size*/ = 0);
}; // class NandDevice04

#endif //__NAND_DEVICE_04_H_INCLUDED__
