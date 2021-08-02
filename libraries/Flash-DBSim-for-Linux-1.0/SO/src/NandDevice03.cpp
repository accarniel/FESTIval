#include "stdafx.h"
#include "NandDevice03.h"

#define PAGE_INDEX(blockID, pageID)	(blockID * info.pageCountPerBlock + pageID)

//int readTime = 0;
//int blockSize = 0;

/* Constructor of NandDevice03 */
NandDevice03::NandDevice03(void)
{
	vfSpace = NULL;
}

/* Destructor of NandDevice03 */
NandDevice03::~NandDevice03(void)
{
	if (vfSpace) { delete [] vfSpace; vfSpace = NULL; }
}

/* Query Interface */
RV NandDevice03::QueryInterface(const IID& iid,	/* ID of interface */
								void ** ppv)	/* interface pointer */
{
	if (NandDevice02::QueryInterface(iid, ppv))
		return RV_ERROR_UNSUPPORT_INTERFACE;

	return RV_OK;
}

/* Initialize the 'NandDevice03' device
 */
RV NandDevice03::Initialize(const VFD_INFO& info)
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
	int pageCount = this->info.blockCount * this->info.pageCountPerBlock;
	int flashSize = pageCount * this->info.pageSize;

	eraseCounter = new int [info.blockCount];
	readCounter = new int [pageCount];
	writeCounter = new int [pageCount];
	memset(eraseCounter, 0, info.blockCount * sizeof(int));
	memset(readCounter, 0, pageCount * sizeof(int));
	memset(writeCounter, 0, pageCount * sizeof(int));

	// initialize latency info
	readLatencyTotal = writeLatencyTotal = eraseLatencyTotal = 0;

	// initialize virtual flash storage space in memory
	vfSpace = new BYTE [flashSize];
	memset(vfSpace, 0xFF, flashSize);	// ALL BITS ARE '1'

	return RV_OK;
}

/* Release the 'NandDevice01' device
 */
RV NandDevice03::Release(void)
{
	if (eraseCounter) { delete [] eraseCounter; eraseCounter = NULL; }
	if (readCounter) { delete [] readCounter; readCounter = NULL; }
	if (writeCounter) { delete [] writeCounter; writeCounter = NULL; }

	if (vfSpace) { delete [] vfSpace; vfSpace = NULL; }

	return RV_OK;
}

/* Erase specified block
 */
RV NandDevice03::EraseBlock(BLOCK_ID blockID)
{
	ASSERT(eraseCounter);
	ASSERT(blockID >= 0 && blockID < info.blockCount);

	if (eraseCounter[blockID] >= info.eraseLimitation) {
		return RV_ERROR_FLASH_BLOCK_BROKEN;
	}

	// erase process
	int blockSize = this->info.pageSize * this->info.pageCountPerBlock;
	memset(vfSpace + blockID * blockSize, 0xFF, blockSize);

	eraseCounter[blockID] ++;
	eraseLatencyTotal += info.eraseTime;

	return RV_OK;
}

/* Read data from specified page
 */
RV NandDevice03::ReadPage(BLOCK_ID blockID,
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
	int blockSize = this->info.pageSize * this->info.pageCountPerBlock;
	memcpy(buffer, vfSpace + blockID * blockSize + pageID * info.pageSize, info.pageSize);

	readCounter[PAGE_INDEX(blockID, pageID)] ++;
	readLatencyTotal += this->info.readTime.randomTime + this->info.readTime.serialTime * this->info.pageSize;

	return RV_OK;
}

/* Write data to specified page
 */
RV NandDevice03::WritePage(BLOCK_ID blockID,
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
	int blockSize = this->info.pageSize * this->info.pageCountPerBlock;
	for (int i = 0, pos = blockID * blockSize + pageID * info.pageSize; i < info.pageSize; i++, pos++) {
		*(vfSpace + pos) &= buffer[i];
	}

	writeCounter[PAGE_INDEX(blockID, pageID)] ++;
	writeLatencyTotal += info.programTime;

	return RV_OK;
}
