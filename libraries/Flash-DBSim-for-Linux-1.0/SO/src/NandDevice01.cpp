#include "stdafx.h"
#include "NandDevice01.h"

#define PAGE_INDEX(blockID, pageID)	(blockID * info.pageCountPerBlock + pageID)

/* Constructor of NandDevice01 */
NandDevice01::NandDevice01(void)
{
	// :)
}

/* Destructor of NandDevice01 */
NandDevice01::~NandDevice01(void)
{
	// :)
}

/* Query Interface (COM?!) */
RV NandDevice01::QueryInterface(const IID& iid,	/* ID of interface */
								void ** ppv)	/* interface pointer */
{
	if (GUID_equals(iid, IID_IUnknown)) {
		* ppv = static_cast<IVFD *>(this);
	} else if (GUID_equals(iid, IID_IVFD)) {
		* ppv = static_cast<IVFD *>(this);
	} else if (GUID_equals(iid, IID_IVFD_COUNTER)) {
		* ppv = static_cast<IVFD_COUNTER *>(this);
	} else {
		* ppv = NULL;
		return RV_ERROR_UNSUPPORT_INTERFACE;
	}

	return RV_OK;
}

/* Get read count of specified page */
int NandDevice01::GetReadCount(BLOCK_ID blockID, 
							   PAGE_ID pageID)
{
	ASSERT(readCounter);
	ASSERT(blockID >= 0 && blockID < info.blockCount);
	ASSERT(pageID >= 0 && pageID < info.pageCountPerBlock);

	return readCounter[PAGE_INDEX(blockID, pageID)];
}

/* Get write count of specified page */
int NandDevice01::GetWriteCount(BLOCK_ID blockID, 
								PAGE_ID pageID)
{
	ASSERT(writeCounter);
	ASSERT(blockID >= 0 && blockID < info.blockCount);
	ASSERT(pageID >= 0 && pageID < info.pageCountPerBlock);

	return writeCounter[PAGE_INDEX(blockID, pageID)];
}

/* Get erase count of specified page */
int NandDevice01::GetEraseCount(BLOCK_ID blockID)
{
	ASSERT(eraseCounter);
	ASSERT(blockID >= 0 && blockID < info.blockCount);

	return eraseCounter[blockID];
}

/* Get total read count */
int NandDevice01::GetReadCountTotal(void)
{
	int total = 0;

	for (int i = 0; i < info.blockCount; i++) {
		for (int j = 0; j < info.pageCountPerBlock; j++) {
			total += readCounter[PAGE_INDEX(i, j)];
		}
	}

	return total;
}

/* Get total write count */
int NandDevice01::GetWriteCountTotal(void)
{
	int total = 0;

	for (int i = 0; i < info.blockCount; i++) {
		for (int j = 0; j < info.pageCountPerBlock; j++) {
			total += writeCounter[PAGE_INDEX(i, j)];
		}
	}

	return total;
}

/* Get total erase count */
int NandDevice01::GetEraseCountTotal(void)
{
	int total = 0;

	for (int i = 0; i < info.blockCount; i++) {
		total += eraseCounter[i];
	}

	return total;
}

/* reset read count */
void NandDevice01::ResetReadCount(void)
{
	int pageCount = info.blockCount * info.pageCountPerBlock;
	memset(readCounter, 0, pageCount * sizeof(int));
}

/* Reset write count */
void NandDevice01::ResetWriteCount(void)
{
	int pageCount = info.blockCount * info.pageCountPerBlock;
	memset(writeCounter, 0, pageCount * sizeof(int));
}

/* Reset erase count */
void NandDevice01::ResetEraseCount(void)
{
	memset(eraseCounter, 0, info.blockCount * sizeof(int));
}

/* Reset read/write/erase counter */
void NandDevice01::ResetCounter(void)
{
	ResetReadCount();
	ResetWriteCount();
	ResetEraseCount();
}

/* Initialize the 'NandDevice01' device
 */
RV NandDevice01::Initialize(const VFD_INFO& info)
{
	Release();	// prerelease

	// initialize module information
	this->info.blockCount = info.blockCount;
	this->info.pageCountPerBlock = info.pageCountPerBlock;
	this->info.eraseLimitation = info.eraseLimitation;
	// other information are useless...
	
	// initialize counters
	int pageCount = info.blockCount * info.pageCountPerBlock;

	eraseCounter = new int [info.blockCount];
	readCounter = new int [pageCount];
	writeCounter = new int [pageCount];
	memset(eraseCounter, 0, info.blockCount * sizeof(int));
	memset(readCounter, 0, pageCount * sizeof(int));
	memset(writeCounter, 0, pageCount * sizeof(int));

	return RV_OK;
}

/* Release the 'NandDevice01' device
 */
RV NandDevice01::Release(void)
{
	if (eraseCounter) { delete [] eraseCounter; eraseCounter = NULL; }
	if (readCounter) { delete [] readCounter; readCounter = NULL; }
	if (writeCounter) { delete [] writeCounter; writeCounter = NULL; }

	return RV_OK;
}

/* Erase specified block
 */
RV NandDevice01::EraseBlock(BLOCK_ID blockID)
{
	ASSERT(eraseCounter);
	ASSERT(blockID >= 0 && blockID < info.blockCount);

	if (eraseCounter[blockID] >= info.eraseLimitation) {
		return RV_ERROR_FLASH_BLOCK_BROKEN;
	}

	// erase process
	eraseCounter[blockID] ++;

	return RV_OK;
}

/* Read data from specified page
 */
RV NandDevice01::ReadPage(BLOCK_ID blockID,
						  PAGE_ID pageID,
						  BYTE * buffer,
						  int offset,
						  int size)
{
	ASSERT(eraseCounter);
	ASSERT(blockID >= 0 && blockID < info.blockCount);
	ASSERT(pageID >= 0 && pageID < info.pageCountPerBlock);

	if (eraseCounter[blockID] >= info.eraseLimitation) {
		return RV_ERROR_FLASH_BLOCK_BROKEN;
	}

	// read process
	readCounter[PAGE_INDEX(blockID, pageID)] ++;

	return RV_OK;
}

/* Write data to specified page
 */
RV NandDevice01::WritePage(BLOCK_ID blockID,
						   PAGE_ID pageID,
						   const BYTE * buffer,
						   int offset,
						   int size)
{
	ASSERT(eraseCounter);
	ASSERT(blockID >= 0 && blockID < info.blockCount);
	ASSERT(pageID >= 0 && pageID < info.pageCountPerBlock);

	if (eraseCounter[blockID] >= info.eraseLimitation) {
		return RV_ERROR_FLASH_BLOCK_BROKEN;
	}

	// write process
	writeCounter[PAGE_INDEX(blockID, pageID)] ++;

	return RV_OK;
}
