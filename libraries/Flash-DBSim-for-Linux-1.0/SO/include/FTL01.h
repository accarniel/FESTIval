/*
 * Flash-DBSim Storage Simulation Environment
 * FTL01.h - FTL algorithm, type 01. See also 'FTL01.readme'.
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

#ifndef __FTL_01_H_INCLUDED__
#define __FTL_01_H_INCLUDED__

#include "interface.h"

#include <vector>

using namespace std;

/* Physical Block Address */
typedef struct PBA {
	BLOCK_ID	blockID;
	PAGE_ID		pageID;

	/* Constructors */
public:
	PBA(void) : blockID(-1), pageID(-1) {}
	PBA(BLOCK_ID bID, PAGE_ID pID) : blockID(bID), pageID(pID) {}
} PBA;

/* Data state of each page */
typedef enum PAGE_STATE {
	FREE = 0,	/* I'm free, I'm free...(repeat) */
	ALLC = 1,	/* Allocated page(seriously) */
	LIVE = 2,	/* Yeah, I'm alive */
	DEAD = 3,	/* Errrr, am I dead?! */
} PS;

/* Block states */
typedef struct BLOCK_STATE {
	int			freePages : 8;	/* The count of free pages */
	int			livePages : 8;	/* The count of live pages */
	int			deadPages : 8;	/* The count of dead pages */
	int			allocatedPages : 8;	/* The count of allocated pages */

	PAGE_STATE * pageStates;	/* The states of pages in block, 2 bits indicate 1 state */
} BS;

/* FTL01. See also 'FTL01.readme'
 */
class FTL01 : public IFTL
{
protected:
	BLOCK_STATE *	blockState;	/* Block states */

	PBA *			mapList;	/* LBA-PBA map list */
	LBA *			reverseMapList;	/* PBA-LBA map list */
	int				mapListPoint;	/* point to a specified position, used to find next empty LBA faster */
	
	vector<BLOCK_ID>	freeList;	/* Free block list */
	vector<BLOCK_ID>	dirtyList;	/* Dirty block list */
	vector<BLOCK_ID>	deadList;	/* Dead block list */

	BLOCK_ID		reserved;	/* reserved block for wear-leveling */

	/* Constructor & Destructor */
public:
	FTL01(void);
	~FTL01(void);

public:
	virtual RV QueryInterface(const IID& /*iid*/, void ** /*ppv*/);

public:
	/* Methods */
	virtual RV Initialize(const FTL_INFO& /*info*/, const IVFD * /*device*/);
	virtual RV Release(void);

	virtual int AllocPage(int /*count*/, LBA * /*lbas*/);
	virtual RV ReleasePage(LBA /*lba*/);
	virtual RV ReadPage(LBA /*lba*/, BYTE * /*buffer*/, int /*offset*/, size_t /*size*/);
	virtual RV WritePage(LBA /*lba*/, const BYTE * /*buffer*/, int /*offset*/, size_t /*size*/);

protected:
	virtual PBA TranslateLBAtoPBA(LBA /*lba*/);
	virtual RV RegisterEntry(LBA /*lba*/, PBA /*pba*/);
	virtual PBA AllocNewPage(void);
	virtual RV ReclaimBlock(void);

	PAGE_STATE GetPageState(PBA /*pba*/);
	void SetPageState(PBA /*pba*/, PAGE_STATE /*ps*/);

	void MoveDirtyToDead(BLOCK_ID /*blockID*/);
}; // class FTL01

#endif //__FTL_01_H_INCLUDED__
