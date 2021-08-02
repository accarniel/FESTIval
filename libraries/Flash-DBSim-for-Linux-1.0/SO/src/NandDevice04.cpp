#include "stdafx.h"
#include "NandDevice04.h"

#define VFIMAGE		"flash.image"	/* virtual flash storge in disk image */

#define PAGE_INDEX(blockID, pageID)	(blockID * info.pageCountPerBlock + pageID)

int readTime = 0;
int blockSize = 0;

/* Constructor of NandDevice04 */
NandDevice04::NandDevice04(void)
{
}

/* Destructor of NandDevice04 */
NandDevice04::~NandDevice04(void)
{
	if (vfImage.is_open()) vfImage.close();
}

/* Query Interface */
RV NandDevice04::QueryInterface(const IID& iid,	/* ID of interface */
								void ** ppv)	/* interface pointer */
{
	if (NandDevice02::QueryInterface(iid, ppv))
		return RV_ERROR_UNSUPPORT_INTERFACE;

	return RV_OK;
}

/* Initialize the 'NandDevice04' device
 */
RV NandDevice04::Initialize(const VFD_INFO& info)
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
	
	// set static variables
	blockSize = this->info.pageSize * info.pageCountPerBlock;
	readTime = info.readTime.randomTime + info.readTime.serialTime * this->info.pageSize;

	// initialize counters
	int pageCount = info.blockCount * info.pageCountPerBlock;
	int flashSize = this->info.pageSize * pageCount;

	eraseCounter = new int [info.blockCount];
	readCounter = new int [pageCount];
	writeCounter = new int [pageCount];
	memset(eraseCounter, 0, info.blockCount * sizeof(int));
	memset(readCounter, 0, pageCount * sizeof(int));
	memset(writeCounter, 0, pageCount * sizeof(int));

	// initialize latency info
	readLatencyTotal = writeLatencyTotal = eraseLatencyTotal = 0;

	// initialize virtual flash storage space in disk
	vfImage.open(VFIMAGE, ios::binary | ios::in | ios::out | ios::trunc);
	BYTE * buffer = new BYTE[this->info.pageSize];
	for (int i = 0; i < this->info.pageSize; i++) {
		buffer[i] = 0x00;
	}
	for (int i = 0; i < this->info.blockCount * this->info.pageCountPerBlock; i++) {
		vfImage.write((const char *)buffer, this->info.pageSize);
	}
	vfImage.flush();
	delete [] buffer;

	return RV_OK;
}

/* Release the 'NandDevice01' device
 */
RV NandDevice04::Release(void)
{
	if (eraseCounter) delete [] eraseCounter;
	if (readCounter) delete [] readCounter;
	if (writeCounter) delete [] writeCounter;

	if (vfImage.is_open()) vfImage.close();

	return RV_OK;
}

/* Erase specified block
 */
RV NandDevice04::EraseBlock(BLOCK_ID blockID)
{
	ASSERT(eraseCounter);
	ASSERT(blockID >= 0 && blockID < info.blockCount);

	if (eraseCounter[blockID] >= info.eraseLimitation) {
		return RV_ERROR_FLASH_BLOCK_BROKEN;
	}

	// erase process
	vfImage.seekp(blockID * blockSize, ios::beg);
	for (int i = 0; i < blockSize; i++) {
		vfImage.put(0x00);
	}
	vfImage.flush();

	eraseCounter[blockID] ++;
	eraseLatencyTotal += info.eraseTime;

	return RV_OK;
}

/* Read data from specified page
 */
RV NandDevice04::ReadPage(BLOCK_ID blockID,
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
	vfImage.seekg(blockID * blockSize + pageID * info.pageSize, ios::beg);
	vfImage.read((char *)buffer, info.pageSize);
	vfImage.flush();

	readCounter[PAGE_INDEX(blockID, pageID)] ++;
	readLatencyTotal += readTime;

	return RV_OK;
}

/* Write data to specified page
 */
RV NandDevice04::WritePage(BLOCK_ID blockID,
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
	vfImage.seekg(blockID * blockSize + pageID * info.pageSize, ios::beg);
	vfImage.write((const char *)buffer, info.pageSize);
	vfImage.flush();

	writeCounter[PAGE_INDEX(blockID, pageID)] ++;
	writeLatencyTotal += info.programTime;

	return RV_OK;
}
