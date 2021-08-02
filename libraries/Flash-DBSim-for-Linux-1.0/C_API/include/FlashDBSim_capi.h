// FlashDBSimDll.h : 定义 DLL 应用程序的导出函数。
//
#ifndef __FLASH_DBSIM_cAPI_H_INCLUDED__
#define __FLASH_DBSIM_cAPI_H_INCLUDED__

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include <stdio.h>

typedef struct FTL_INFO FTL_INFO_t;
typedef struct VFD_INFO VFD_INFO_t;

typedef void* IVFD_COUNTER_t;
typedef void* IVFD_LATENCY_t;

typedef unsigned char	BYTE;

typedef int				RV;
typedef int				BLOCK_ID;
typedef int				PAGE_ID;
typedef int				LBA;	/* Logical Block Address */

typedef int				ID_MODULE;
typedef ID_MODULE		IDM_VFD, IDM_MTD, IDM_FTL;

/* IDs of VFD modules */
typedef enum /*VFD_ID*/ {
	ID_VFD_NONE_t,

	ID_NAND_DEVICE_01_t,	/* NandDevice01 module */
	ID_NAND_DEVICE_02_t,	/* NandDevice02 module */
	ID_NAND_DEVICE_03_t,	/* NandDevice03 module */
	ID_NAND_DEVICE_04_t,	/* NandDevice04 module */
} VFD_ID_t;

/* IDs of FTL modules */
typedef enum /*FTL_ID*/ {
	ID_FTL_NONE_t,
	
	ID_FTL_01_t,	/* FTL01 module */
} FTL_ID_t;

EXTERNC FTL_INFO_t * create_ftl_info(IDM_FTL id, int mapListSize, int wearLevelingThreshold);

EXTERNC void check_ftl_info(FTL_INFO_t* ftlInfo);

EXTERNC VFD_INFO_t * create_vfd_info(IDM_VFD id, int blockCount, int pageCountPerBlock, int pageSize1, int pageSize2, int eraseLimitation, int readRandomTime, int readSerialTime, int programTime, int eraseTime);

EXTERNC void check_vfd_info(VFD_INFO_t* vfdInfo);

EXTERNC RV f_initialize_c(const VFD_INFO_t* vfdInfo, const FTL_INFO_t* ftlInfo);

EXTERNC RV f_release_c(void);

EXTERNC RV f_alloc_page_c(int count, LBA * lbas);

EXTERNC RV f_release_page_c(LBA lba);

EXTERNC RV f_read_page_c(LBA lba, BYTE * buffer, int offset, size_t size);

EXTERNC RV f_write_page_c(LBA lba, const BYTE * buffer, int offset, size_t size);

EXTERNC IVFD_COUNTER_t f_get_vfd_counter_c(void);

EXTERNC int f_get_read_count_total_c(IVFD_COUNTER_t icounter);
EXTERNC int f_get_write_count_total_c(IVFD_COUNTER_t icounter);
EXTERNC int f_get_erase_count_total_c(IVFD_COUNTER_t icounter);

EXTERNC int f_get_read_count_c(IVFD_COUNTER_t icounter, BLOCK_ID blockID, PAGE_ID pageID);
EXTERNC int f_get_write_count_c(IVFD_COUNTER_t icounter, BLOCK_ID blockID, PAGE_ID pageID);
EXTERNC int f_get_erase_count_c(IVFD_COUNTER_t icounter, BLOCK_ID blockID);

EXTERNC void f_reset_read_count_c(IVFD_COUNTER_t icounter);
EXTERNC void f_reset_write_count_c(IVFD_COUNTER_t icounter);
EXTERNC void f_reset_erase_count_c(IVFD_COUNTER_t icounter);
EXTERNC void f_reset_counter_c(IVFD_COUNTER_t icounter);

EXTERNC IVFD_LATENCY_t f_get_vfd_latency_c(void);

EXTERNC int f_get_read_latency_total_c(IVFD_LATENCY_t ilatency);
EXTERNC int f_get_write_latency_total_c(IVFD_LATENCY_t ilatency);
EXTERNC int f_get_erase_latency_total_c(IVFD_LATENCY_t ilatency);

EXTERNC void f_reset_read_latency_total_c(IVFD_LATENCY_t ilatency);
EXTERNC void f_reset_write_latency_total_c(IVFD_LATENCY_t ilatency);
EXTERNC void f_reset_erase_latency_total_c(IVFD_LATENCY_t ilatency);
EXTERNC void f_reset_latency_total_c(IVFD_LATENCY_t ilatency);

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

#endif
