#include "stdafx.h"
#include "NandDevice02.h"

#define PAGE_INDEX(blockID, pageID)	(blockID * info.pageCountPerBlock + pageID)

//int readTime = 0;

/* Constructor of NandDevice02 */
NandDevice02::NandDevice02(void)
{
	// :)
}

/* Destructor of NandDevice02 */
NandDevice02::~NandDevice02(void)
{
	// :)
}

/* Query Interface */
RV NandDevice02::QueryInterface(const IID& iid,	/* ID of interface */
								void ** ppv)	/* interface pointer */
{
	/*if (iid == IID_IUnknown) {
		* ppv = static_cast<IVFD *>(this);
	} else if (iid == IID_IVFD) {
		* ppv = static_cast<IVFD *>(this);
	} else if (iid == IID_IVFD_COUNTER) {
		* ppv = static_cast<IVFD_COUNTER *>(this);
	} else if (iid == IID_VFD_LATENCY) {
		* ppv = static_cast<IVFD_LATENCY *>(this);
	}*/
	if (GUID_equals(iid, IID_IVFD_LATENCY)) {
		* ppv = static_cast<IVFD_LATENCY *>(this);
	} else {
		if (NandDevice01::QueryInterface(iid, ppv))
			return RV_ERROR_UNSUPPORT_INTERFACE;
		//return NandDevice01::QueryInterface(iid, ppv);
	}

	return RV_OK;
}

/* Initialize the 'NandDevice02' device
 */
RV NandDevice02::Initialize(const VFD_INFO& info)
{
	Release();

	// initialize module information
	this->info.blockCount = info.blockCount;
	this->info.pageCountPerBlock = info.pageCountPerBlock;
	this->info.pageSize = info.pageSize;
	this->info.eraseLimitation = info.eraseLimitation;

	this->info.readTime = info.readTime;
	this->info.programTime = info.programTime;
	this->info.eraseTime = info.eraseTime;
	
	// initialize counters
	int pageCount = info.blockCount * info.pageCountPerBlock;

	eraseCounter = new int [info.blockCount];
	readCounter = new int [pageCount];
	writeCounter = new int [pageCount];
	memset(eraseCounter, 0, info.blockCount * sizeof(int));
	memset(readCounter, 0, pageCount * sizeof(int));
	memset(writeCounter, 0, pageCount * sizeof(int));

	// initialize latency info
	readLatencyTotal = writeLatencyTotal = eraseLatencyTotal = 0;

	return RV_OK;
}

/* Release the 'NandDevice01' device
 */
RV NandDevice02::Release(void)
{
	if (eraseCounter) { delete [] eraseCounter; eraseCounter = NULL; }
	if (readCounter) { delete [] readCounter; readCounter = NULL; }
	if (writeCounter) { delete [] writeCounter; writeCounter = NULL; }

	readLatencyTotal = writeLatencyTotal = eraseLatencyTotal = 0;

	return RV_OK;
}

/* Erase specified block
 */
RV NandDevice02::EraseBlock(BLOCK_ID blockID)
{
	ASSERT(eraseCounter);
	ASSERT(blockID >= 0 && blockID < info.blockCount);

	if (eraseCounter[blockID] >= info.eraseLimitation) {
		return RV_ERROR_FLASH_BLOCK_BROKEN;
	}

	// erase process
	eraseCounter[blockID] ++;
	eraseLatencyTotal += info.eraseTime;

	return RV_OK;
}

/* Read data from specified page
 */
RV NandDevice02::ReadPage(BLOCK_ID blockID,
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
	readLatencyTotal += this->info.readTime.randomTime + this->info.readTime.serialTime * this->info.pageSize;

	return RV_OK;
}

/* Write data to specified page
 */
RV NandDevice02::WritePage(BLOCK_ID blockID,
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
	writeLatencyTotal += info.programTime;

	return RV_OK;
}
