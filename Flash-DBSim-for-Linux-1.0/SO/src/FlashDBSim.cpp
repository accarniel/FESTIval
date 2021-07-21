#include "stdafx.h"
#include "FlashDBSim.h"

#include "NandDevice01.h"
#include "NandDevice02.h"
#include "NandDevice03.h"
#include "NandDevice04.h"

#include "FTL01.h"

IFTL * FlashDBSim::ftl = NULL;

/* Initialization of Flash-DBSim */
RV FlashDBSim::Initialize(const VFD_INFO& vfdInfo,
						  const FTL_INFO& ftlInfo)
{
	RV rv;

	FlashDBSim::Release();

	IVFD * flashDevice = NULL;

	// create specified VFD module
	switch (vfdInfo.id) {
		case ID_NAND_DEVICE_01:
			flashDevice = new NandDevice01();
			break;
		case ID_NAND_DEVICE_02:
			flashDevice = new NandDevice02();
			break;
		case ID_NAND_DEVICE_03:
			flashDevice = new NandDevice03();
			break;
		case ID_NAND_DEVICE_04:
			flashDevice = new NandDevice04();
			break;
		default:
			//flashDevice = NULL;
			break;
	} // switch (vfdInfo.id)

	if (!flashDevice) return RV_ERROR_WRONG_MODULE_ID;

	// initialize VFD module
	rv = flashDevice->Initialize(vfdInfo);

	if (rv != RV_OK) {
		delete flashDevice;
		return RV_ERROR_MODULE_INITIALIZE_FAILED;
	}

	// create specified FTL module
	ftl = NULL;

	switch (ftlInfo.id) {
		case ID_FTL_01:
			ftl = new FTL01();
			break;
		default:
			//ftl = NULL;
			break;
	} // switch (ftlInfo.id)

	if (!ftl) {
		flashDevice->Release(); delete flashDevice;
		return RV_ERROR_WRONG_MODULE_ID;
	}

	// initialize FTL module
	rv = ftl->Initialize(ftlInfo, flashDevice);

	if (rv != RV_OK) {
		flashDevice->Release(); delete flashDevice;
		delete ftl; ftl = NULL;
		return RV_ERROR_MODULE_INITIALIZE_FAILED;
	}

	return RV_OK;
}

/* Release the Flash-DBSim System */
RV FlashDBSim::Release(void)
{
	if (!ftl) return RV_OK;

	IVFD * device = const_cast<IVFD *>(ftl->GetFlashDevice());

	ftl->Release(); delete ftl; ftl = NULL;
	device->Release(); delete device;

	return RV_OK;
}

/* Allocate a number of pages */
int FlashDBSim::AllocPage(int count,
						  LBA * lbas)
{
	ASSERT(ftl && lbas);

	return ftl->AllocPage(count, lbas);
}

/* Release specified page */
RV FlashDBSim::ReleasePage(LBA lba)
{
	ASSERT(ftl);

	return ftl->ReleasePage(lba);
}

/* Read data from specified page */
RV FlashDBSim::ReadPage(LBA lba,
						BYTE * buffer,
						int offset,
						size_t size)
{
	ASSERT(ftl);

	return ftl->ReadPage(lba, buffer, offset, size);
}

/* Write data to specified page */
RV FlashDBSim::WritePage(LBA lba,
						 const BYTE * buffer,
						 int offset,
						 size_t size)
{
	ASSERT(ftl);

	return ftl->WritePage(lba, buffer, offset ,size);
}
