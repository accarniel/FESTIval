/*
 * Flash-DBSim Storage Simulation Environment
 * stdafx.h - Standard constant, type definitions, and so on.
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

#ifndef __STDAFX_H_INCLUDED__
#define __STDAFX_H_INCLUDED__

// #include "targetver.h"
#include "limits.h"
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头中排除极少使用的资料

/*#include <windows.h>
*/
#ifdef _DEBUG
#include <assert.h>
#define ASSERT(booleanExpression)	assert(booleanExpression)
#else
#define ASSERT(booleanExpression)
#endif //_DEBUG

#ifndef DLLEXPORT
#define DLLEXPORT	_declspec(dllexport)
#endif //DLLEXPORT

typedef unsigned char	BYTE;

typedef int				RV;
typedef int				BLOCK_ID;
typedef int				PAGE_ID;
typedef void *			PVOID;	/* void pointer */

typedef int				LBA;	/* Logical Block Address */

typedef int				ID_MODULE;
typedef ID_MODULE		IDM_VFD, IDM_MTD, IDM_FTL;

#ifndef INTERFACE
#define INTERFACE		class	/* interface identifier */
#endif //INTERFACE

typedef struct _GUID {
   unsigned long Data1;
   unsigned short Data2;
   unsigned short Data3;
   BYTE Data4[8];
 } GUID;
typedef GUID IID;

unsigned int GUID_equals(IID iid1, IID iid2);

/* Flash device type: NAND/NOR */
typedef enum FLASH_TYPE {
	NAND	= 0,	/* NAND type flash */
	NOR		= 1,	/* NOR type flash */
} FLASH_TYPE;

/* IDs of VFD modules */
typedef enum /*VFD_ID*/ {
	ID_VFD_NONE,

	ID_NAND_DEVICE_01,	/* NandDevice01 module */
	ID_NAND_DEVICE_02,	/* NandDevice02 module */
	ID_NAND_DEVICE_03,	/* NandDevice03 module */
	ID_NAND_DEVICE_04,	/* NandDevice04 module */
} VFD_ID;

/* IDs of FTL modules */
typedef enum /*FTL_ID*/ {
	ID_FTL_NONE,
	
	ID_FTL_01,	/* FTL01 module */
} FTL_ID;

/* IUnknown Interface */
typedef INTERFACE IUnknown {
public:
	virtual RV QueryInterface(const IID& /*iid*/, void ** /*ppv*/) = 0;
	//virtual int AddRef(void) = 0;
	//virtual int Release(void) = 0;
} IUnknown; // INTERFACE IUnknown

/* Interface IUnknown */
// {669FBC31-F562-4b05-9FD6-B18B1517DF38}
const IID IID_IUnknown = 
{ 0x669fbc31, 0xf562, 0x4b05, { 0x9f, 0xd6, 0xb1, 0x8b, 0x15, 0x17, 0xdf, 0x38 } };
/* Interface of Virtual Flash Device (VFD) Module */
// {CDF32DDF-02CA-4893-9D13-0FD417234934}
const IID IID_IVFD = 
{ 0xcdf32ddf, 0x2ca, 0x4893, { 0x9d, 0x13, 0xf, 0xd4, 0x17, 0x23, 0x49, 0x34 } };
/* Interfaces of I/O counters for VFD module */
// {661617C9-9640-427e-9D69-4670422E9C79}
const IID IID_IVFD_COUNTER = 
{ 0x661617c9, 0x9640, 0x427e, { 0x9d, 0x69, 0x46, 0x70, 0x42, 0x2e, 0x9c, 0x79 } };
/* Interfaces of I/O latencies for VFD module */
// {C3B4DA4D-221C-44a8-9DB8-B672483FE117}
const IID IID_IVFD_LATENCY = 
{ 0xc3b4da4d, 0x221c, 0x44a8, { 0x9d, 0xb8, 0xb6, 0x72, 0x48, 0x3f, 0xe1, 0x17 } };
/* Interface of Flash Translate Layer Module */
// {1BAC5EDA-18F5-4234-A73C-6411E8392899}
const IID IID_IFTL = 
{ 0x1bac5eda, 0x18f5, 0x4234, { 0xa7, 0x3c, 0x64, 0x11, 0xe8, 0x39, 0x28, 0x99 } };

/************************************************************************/
/* The following section defines a list of return codes. They are returned by the functions defined
 * in 'Flash-DBSim'.
 */
#define RV_OK							0x0	/* Operation completed successfully */
#define RV_FAIL							0x1	/* Operation failed */

#define RV_ERROR_ARRAY_OUT_BOUND		0x2	/* Array out of bound */
#define RV_ERROR_INVALID_TYPE			0x3	/* Invalid data type */
#define RV_ERROR_FILE_IO				0x4	/* File I/O Exception */

#define RV_ERROR_FLASH_IO_FAILED		0x1000	/* Flash I/O operations are failed */
#define RV_ERROR_FLASH_BLOCK_BROKEN		0x1001	/* One block of flash device has broken */
#define RV_ERROR_FLASH_NO_MEMORY		0x1002	/* No more flash memory */
#define RV_ERROR_FLASH_NOT_DIRTY		0x1003	/* Flash memory has no dirty page */
#define RV_ERROR_FLASH_IO_OVERFLOW		0x1004	/* Flash I/O operations are overflow with invalid offset or size */

#define RV_ERROR_INVALID_LBA			0x2000	/* Invalid LBA */
#define RV_ERROR_INVALID_PAGE_STATE		0x2001	/* Invalid page state */

#define RV_ERROR_WRONG_MODULE_ID		0x3000	/* Wrong Flash-DBSim module ID */
#define RV_ERROR_MODULE_INITIALIZE_FAILED	\
										0x3001	/* Flash-DBSim module initialization failed */

#define RV_ERROR_UNSUPPORT_OBJECT		0x10000	/* Unsupported object */
#define RV_ERROR_UNSUPPORT_INTERFACE	0x12345	/* Unsupported interface */

#endif //__STDAFX_H_INCLUDED__
