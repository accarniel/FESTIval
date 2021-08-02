// FlashDBSimDll.cpp : ¶¨Òå DLL Ó¦ÓÃ³ÌÐòµÄµ¼³öº¯Êý¡£
//
#include "FlashDBSimSO.h"

RV f_initialize(const VFD_INFO& vfdInfo, const FTL_INFO& ftlInfo) {
	return FlashDBSim::Initialize(vfdInfo, ftlInfo);
}

RV f_release(void) {
	return FlashDBSim::Release();
}

int f_alloc_page(int count, LBA * lbas) {
	return FlashDBSim::AllocPage(count, lbas);
}

RV f_release_page(LBA lba) {
	return FlashDBSim::ReleasePage(lba);
}

RV f_read_page(LBA lba, BYTE * buffer, int offset, size_t size) {
	return FlashDBSim::ReadPage(lba, buffer, offset, size);
}

RV f_write_page(LBA lba, const BYTE * buffer, int offset, size_t size) {
	return FlashDBSim::WritePage(lba, buffer, offset, size);
}

const IFTL * f_get_ftl_module(void) {
	return FlashDBSim::GetFTLModule();
}

const IVFD * f_get_vfd_module(void) {
	IFTL * ftl = const_cast<IFTL *>(FlashDBSim::GetFTLModule());

	if (ftl) return ftl->GetFlashDevice();
	else return NULL;
}
