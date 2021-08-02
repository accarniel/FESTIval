// FlashDBSimDll.h : 定义 DLL 应用程序的导出函数。
//
#ifndef __FLASH_DBSIM_SO_H_INCLUDED__
#define __FLASH_DBSIM_SO_H_INCLUDED__

#include "FlashDBSim.h"

RV f_initialize(const VFD_INFO& vfdInfo, const FTL_INFO& ftlInfo);

RV f_release(void);

int f_alloc_page(int count, LBA * lbas);

RV f_release_page(LBA lba);

RV f_read_page(LBA lba, BYTE * buffer, int offset, size_t size);

RV f_write_page(LBA lba, const BYTE * buffer, int offset, size_t size);

const IFTL * f_get_ftl_module(void);

const IVFD * f_get_vfd_module(void);

#endif
