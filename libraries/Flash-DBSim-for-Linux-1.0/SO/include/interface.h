/*
 * Flash-DBSim Storage Simulation Environment
 * Interface.h - Global interface declaration of Flash-DBSim
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

#ifndef __INTERFACE_H_INCLUDED__
#define __INTERFACE_H_INCLUDED__

#include "stdafx.h"
#include <string.h>
#include <stdlib.h>

/************************************************************************/
/* VFD Module                                                           */
/************************************************************************/

/* Information of VFD Module
 */
typedef struct VFD_INFO {
	IDM_VFD		id;					/* id of VFD Module, used for FlashDBSim */

	int			blockCount;			/* the number of blocks in flash device */
	struct _PageSize {
		int size1;					/* the size of each page (data area), in bytes */
		int size2;					/* the size of each page (additional area), in bytes */

		operator int(void) { return size1 + size2; }
	
		/* Constructors */
		_PageSize(void) : size1(0), size2(0) {}
		_PageSize(int s1, int s2) : size1(s1), size2(s2) {}
	} pageSize;
	int			pageCountPerBlock;	/* the number of pages in each block */
	int			eraseLimitation;	/* the erase limitation of each block */
	struct _ReadTime {
		int		randomTime;		/* time of random read operation (MAX.) */
		int		serialTime;		/* time of serial access operation (Min.) */

		/* Constructors */
		_ReadTime(void) : randomTime(0), serialTime(0) {}
		_ReadTime(int r, int s) : randomTime(r), serialTime(s) {}
	} readTime;
	int			programTime;		/* time of page program operation */
	int			eraseTime;			/* time of block erase operation */

	/* Constructor */
public:
	VFD_INFO(void) {
		id = 0;
		blockCount = pageSize.size1 = pageSize.size2 = pageCountPerBlock = eraseLimitation = 0;
		readTime.randomTime = readTime.serialTime = programTime = eraseTime = 0;
	}
	VFD_INFO(IDM_VFD _id, int bc, _PageSize ps, int pcpb, int el, _ReadTime rt, int pt, int et) {
		id = _id;
		blockCount = bc; pageSize = ps; pageCountPerBlock = pcpb;
		eraseLimitation = el; readTime = rt;
		programTime = pt; eraseTime = et;
	}
} VFD_INFO;

typedef INTERFACE IVFD_MODULE : public IUnknown {
protected:
	VFD_INFO	info;	/* Module Information */

public:
	/* Attributes */
	VFD_INFO GetModuleInfo(void) { return info; }	/* get VFD module information */
	virtual FLASH_TYPE GetFlashType(void) { return NAND; }	/* get virtual flash device type */

	/* Methods */
	virtual RV Initialize(const VFD_INFO& /*info*/) = 0;	/* Initialize VFD Module */
	virtual RV Release(void) = 0;	/* Release VFD Module */

	virtual RV EraseBlock(BLOCK_ID /*blockID*/) = 0;	/* erase specified block */
	virtual RV ReadPage(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/, BYTE * /*buffer*/, int /*offset*/ = 0, int /*size*/ = 0) = 0;	/* read specified page */
	virtual RV WritePage(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/, const BYTE * /*buffer*/, int /*offset*/ = 0, int /*size*/ = 0) = 0;	/* write specified page */
} IVFD;

typedef INTERFACE IVFD_COUNTER : public IUnknown {
protected:
	int *	readCounter;		/* read counter of each PAGE */
	int	*	writeCounter;		/* write counter of each PAGE */
	int	*	eraseCounter;		/* erase counter of each BLOCK */

	/* Constructor */
protected:
	IVFD_COUNTER(void) { readCounter = writeCounter = eraseCounter = NULL; }
	virtual ~IVFD_COUNTER(void) {
		if (readCounter) { delete [] readCounter; readCounter = NULL; }
		if (writeCounter) { delete [] writeCounter; writeCounter = NULL; }
		if (eraseCounter) { delete [] eraseCounter; eraseCounter = NULL; }
	}

public:
	/* Attributes */
	virtual int GetReadCount(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/) = 0;
	virtual int GetWriteCount(BLOCK_ID /*blockID*/, PAGE_ID /*pageID*/) = 0;
	virtual int GetEraseCount(BLOCK_ID /*blockID*/) = 0;

	virtual int GetReadCountTotal(void) = 0;
	virtual int GetWriteCountTotal(void) = 0;
	virtual int GetEraseCountTotal(void) = 0;

	virtual void ResetReadCount(void) = 0;
	virtual void ResetWriteCount(void) = 0;
	virtual void ResetEraseCount(void) = 0;
	virtual void ResetCounter(void) = 0;
} IVFD_COUNTER;

typedef INTERFACE IVFD_LATENCY : public IUnknown {
protected:
	int	readLatencyTotal;	/* total latency of READ operations */
	int writeLatencyTotal;	/* total latency of WRITE operations */
	int eraseLatencyTotal;	/* total latency of ERASE operations */

	/* Constructor */
protected:
	IVFD_LATENCY(void) { readLatencyTotal = writeLatencyTotal = eraseLatencyTotal = 0; }

public:
	/* Attributes */
	int GetReadLatencyTotal(void) { return readLatencyTotal; }
	int GetWriteLatencyTotal(void) { return writeLatencyTotal; }
	int GetEraseLatencyTotal(void) { return eraseLatencyTotal; }

	void ResetReadLatencyTotal(void) { readLatencyTotal = 0; }
	void ResetWriteLatencyTotal(void) { writeLatencyTotal = 0; }
	void ResetEraseLatencyTotal(void) { eraseLatencyTotal = 0; }
	void ResetLatencyTotal(void) { readLatencyTotal = writeLatencyTotal = eraseLatencyTotal = 0; }
} IVFD_LATENCY;

/************************************************************************/
/* FTL Module                                                           */
/************************************************************************/

/* Information of FTL Module
 */
typedef struct FTL_INFO {
	IDM_FTL		id;	/* id of FTL Module, used for Flash-DBSim */

	int			mapListSize;			/* size of LBA-PBA map list */
	int			wearLevelingThreshold;	/* threshold for wear leveling */
	
	/* Constructor */
public:
	FTL_INFO(void) : id(ID_FTL_NONE), mapListSize(0), wearLevelingThreshold(0) {}
	FTL_INFO(IDM_FTL _id, int mls, int wlt) {
		id = _id; mapListSize = mls; wearLevelingThreshold = wlt;
	}
} FTL_INFO;

typedef INTERFACE IFTL_MODULE : public IUnknown {
protected:
	FTL_INFO	info;	/* Module Information */
	IVFD *		flashDevice;	/* related flash device */

public:
	/* Attributes */
	FTL_INFO& GetModuleInfo(void) { return info; }	/* get FTL module information */
	const IVFD * GetFlashDevice(void) { return flashDevice; }	/* get related flash device */

	/* Methods */
	virtual RV Initialize(const FTL_INFO& /*info*/, const IVFD * /*device*/) = 0;	/* initialize FTL module */
	virtual RV Release(void) = 0;	/* release FTL module */

	virtual int AllocPage(int /*count*/, LBA * /*lbas*/) = 0;	/* apply to allocate some new pages */
	virtual RV ReleasePage(LBA /*lba*/) = 0;	/* release one page */
	virtual RV ReadPage(LBA /*lba*/, BYTE * /*buffer*/, int /*offset*/, size_t /*size*/) = 0;	/* read specified page */
	virtual RV WritePage(LBA /*lba*/, const BYTE * /*buffer*/, int /*offset*/, size_t /*size*/) = 0;	/* write specified page */
} IFTL;

#endif //__INTERFACE_H_INCLUDED__
