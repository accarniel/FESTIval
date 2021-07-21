#include "stdafx.h"
#include "FTL01.h"

/* Constructor of FTL01 */
FTL01::FTL01(void)
{
	flashDevice = NULL;

	blockState = NULL;
	mapList = NULL;
	reverseMapList = NULL;
	mapListPoint = 0;

	reserved = 0;
}

/* Destructor of FTL01 */
FTL01::~FTL01(void)
{
	if (blockState) {
		for (int i = 0; i < flashDevice->GetModuleInfo().blockCount; i++) {
			delete [] blockState[i].pageStates;
		}

		delete [] blockState;
		blockState = NULL;
	}

	if (mapList) { delete [] mapList; mapList = NULL; }
	if (reverseMapList) { delete [] reverseMapList; reverseMapList = NULL; }

	freeList.clear();
	dirtyList.clear();
	deadList.clear();
}

/* Query Interface */
RV FTL01::QueryInterface(const IID& iid,
						 void ** ppv)
{
	if (GUID_equals(iid, IID_IUnknown)) {
		* ppv = static_cast<IFTL *>(this);
	} else {
		* ppv = NULL;
		return RV_ERROR_UNSUPPORT_INTERFACE;
	}

	return RV_OK;
}

/* Initialize the FTL01 module */
RV FTL01::Initialize(const FTL_INFO& info,
					 const IVFD * device)
{
	Release();

	this->flashDevice = const_cast<IVFD *>(device);
	this->info.id = ID_FTL_01;
	this->info.mapListSize = info.mapListSize;
	this->info.wearLevelingThreshold = info.wearLevelingThreshold;

	VFD_INFO vfdInfo = flashDevice->GetModuleInfo();

	// initialize the lba-pba map lists
	mapList = new PBA[info.mapListSize];
	for (int i = 0; i < info.mapListSize; i++) {
		mapList[i].blockID = mapList[i].pageID = -1;
	}
	reverseMapList = new LBA[vfdInfo.pageCountPerBlock * vfdInfo.blockCount];
	for (int i = 0; i < vfdInfo.pageCountPerBlock * vfdInfo.blockCount; i++) {
		reverseMapList[i] = -1;
	}

	mapListPoint = 0;

	// initialize free/dirty block lists
	freeList.clear(); dirtyList.clear(); deadList.clear();
	for (int i = 0; i < vfdInfo.blockCount - 1; i++) {
		freeList.push_back(i);
	}
	reserved = vfdInfo.blockCount - 1;	// set the last block as the reserved block

	// initialize block state array
	blockState = new BLOCK_STATE[vfdInfo.blockCount];
	for (int i = 0; i < vfdInfo.blockCount; i++) {
		blockState[i].freePages = vfdInfo.pageCountPerBlock;
		blockState[i].livePages = blockState[i].deadPages = blockState[i].allocatedPages = 0;
		blockState[i].pageStates = new PAGE_STATE[vfdInfo.pageCountPerBlock];
		for (int j = 0; j < vfdInfo.pageCountPerBlock; j++) {
			blockState[i].pageStates[j]	= FREE;
		}
	}

	return RV_OK;
}

/* Release the FTL01 module */
RV FTL01::Release(void)
{
	if (blockState) {
		for (int i = 0; i < flashDevice->GetModuleInfo().blockCount; i++) {
			delete [] blockState[i].pageStates;
		}

		delete [] blockState;
		blockState = NULL;
	}

	if (mapList) { delete [] mapList; mapList = NULL; }
	if (reverseMapList) { delete [] reverseMapList; reverseMapList = NULL; }
	freeList.clear();
	dirtyList.clear();
	deadList.clear();

	return RV_OK;
}

/* Allocate a new page */
int FTL01::AllocPage(int count,
					 LBA * lbas)
{
	ASSERT(lbas);
	ASSERT(count > 0);

	int allocated = 0;	/* allocated lba count */

	for (int j = 0; j < info.mapListSize; j++) {
		PBA pba = TranslateLBAtoPBA(mapListPoint);

		if (pba.blockID == -1 || pba.pageID == -1) { // empty lba
			// allocate a new page
			pba = AllocNewPage();

			if (pba.blockID == -1 || pba.pageID == -1) {// no more space free
				for (int i = allocated; i < count; i++)
					lbas[i] = -1;
				return allocated;
			}

			lbas[allocated] = mapListPoint;

			// register new mapping entry
			RegisterEntry(mapListPoint, pba);

			allocated ++;
			if (allocated == count) { // allocation complete
				mapListPoint = (mapListPoint + 1) % info.mapListSize;
				return allocated;
			}
		}

		mapListPoint = (mapListPoint + 1) % info.mapListSize;
	}

	for (int i = allocated; i < count; i++)
		lbas[i] = -1;

	return allocated;
}

/* Release specified page */
RV FTL01::ReleasePage(LBA lba)
{
	ASSERT(lba >= 0);	// ASSERT

	PBA pba = TranslateLBAtoPBA(lba);

	if (pba.blockID == -1 || pba.pageID == -1) { // address mapping not exist
		return RV_ERROR_INVALID_LBA;
	}

	PAGE_STATE ps = GetPageState(pba);

	if (ps == FREE || ps == DEAD) return RV_ERROR_INVALID_PAGE_STATE;

	if (ps == ALLC) // reclaim unused page
		SetPageState(pba, FREE);
	else // LIVE
		SetPageState(pba, DEAD);

	// reset lba-pba list
	pba.blockID = pba.pageID = -1;
	RegisterEntry(lba, pba);

	return RV_OK;
}

/* Read data from specified page */
RV FTL01::ReadPage(LBA lba,
				   BYTE * buffer,
				   int offset,
				   size_t size)
{
	ASSERT(flashDevice);	// ASSERT

	int pageSize = flashDevice->GetModuleInfo().pageSize;

	// adjust 'offset' and 'size' parameters
	if (offset > pageSize) return RV_ERROR_FLASH_IO_OVERFLOW;
	if (offset + (int)size > pageSize) return RV_ERROR_FLASH_IO_OVERFLOW;

	PBA pba = TranslateLBAtoPBA(lba);

	if (pba.blockID == -1 || pba.pageID == -1)
		return RV_ERROR_INVALID_LBA;

	if (flashDevice->GetFlashType() == NAND) { // NAND
		BYTE * _buffer = new BYTE[flashDevice->GetModuleInfo().pageSize];
		RV rv = flashDevice->ReadPage(pba.blockID, pba.pageID, _buffer, offset, size);
		
		if (rv != RV_OK) {
			delete _buffer; return rv;
		}

		memcpy(buffer, _buffer + offset, size);

		delete _buffer;

		return RV_OK;
	} else { // NOR
		return flashDevice->ReadPage(pba.blockID, pba.pageID, buffer, offset, size);
	}
}

/* Write data to specified page */
RV FTL01::WritePage(LBA lba,
					const BYTE * buffer,
					int offset,
					size_t size)
{
	ASSERT(flashDevice);
	ASSERT(buffer);

	int pageSize = flashDevice->GetModuleInfo().pageSize;

	// adjust 'offset' and 'size' parameters
	if (offset > pageSize) return RV_ERROR_FLASH_IO_OVERFLOW;
	if (offset + (int)size > pageSize) return RV_ERROR_FLASH_IO_OVERFLOW;

	PBA pba = TranslateLBAtoPBA(lba);

	if (pba.blockID == -1 || pba.pageID == -1)
		return RV_ERROR_INVALID_LBA;

	PAGE_STATE ps = GetPageState(pba);

	if (ps == DEAD || ps == FREE) {
		return RV_ERROR_INVALID_PAGE_STATE;
	}

	if (ps == LIVE) { // allocate new page
		PBA pba2 = AllocNewPage();
		
		pba = TranslateLBAtoPBA(lba);	// AllocNewPage operation may change the pba value, fetch it again

		if (pba2.blockID == -1 || pba2.pageID == -1) // no free page for writing
			return RV_ERROR_FLASH_NO_MEMORY;

		SetPageState(pba, DEAD);
		RegisterEntry(lba, pba2);	// register new lba-pba
		pba = pba2;
	}

	RV rv;

	if (flashDevice->GetFlashType() == NAND) { // NAND
		BYTE * _buffer = new BYTE[pageSize];
		memset(_buffer, 0x00, pageSize);
		memcpy(_buffer + offset, buffer, size);
		rv = flashDevice->WritePage(pba.blockID, pba.pageID, _buffer, offset, size);

		delete _buffer;
	} else { // NOR
		rv = flashDevice->WritePage(pba.blockID, pba.pageID, buffer, offset, size);
	}

	if (rv != RV_OK) return rv;

	// set related data
	SetPageState(pba, LIVE);

	return RV_OK;
}

/* Translate the specified LBA to relevant PBA */
PBA FTL01::TranslateLBAtoPBA(LBA lba)
{
	ASSERT(lba >= 0 && lba < info.mapListSize);	// ASSERT

	PBA pba = mapList[lba];
	if (pba.blockID != -1 && pba.pageID != -1) {
		ASSERT(reverseMapList[pba.blockID * flashDevice->GetModuleInfo().pageCountPerBlock + pba.pageID] == lba);
	}

	return mapList[lba];
}

/* Register a new (LBA, PBA) entry into map list */
RV FTL01::RegisterEntry(LBA lba,
						PBA pba)
{
	ASSERT(lba >= 0 && lba < info.mapListSize);	// ASSERT

	PBA p = mapList[lba];
	int pageCountPerBlock = flashDevice->GetModuleInfo().pageCountPerBlock;

	if (p.blockID != -1 && p.pageID != -1) {
		reverseMapList[p.blockID * pageCountPerBlock + p.pageID] = -1;
	}

	mapList[lba] = pba;
	if (pba.blockID != -1 && pba.pageID != -1) {
		reverseMapList[pba.blockID * pageCountPerBlock + pba.pageID] = lba;		
	}

	return RV_OK;
}

/* Allocate a new page */
PBA FTL01::AllocNewPage(void)
{
	/* The strategy of space allocation here is:
	 * If there is free page in current block(the last block in dirtyList), then allocate the page,
	 * otherwise, allocate a new page in freeList. If there is no free block in freeList, then,
	 * a garbage collection will be activated.
	 */
	PBA pba;
	int pageCountPerBlock = flashDevice->GetModuleInfo().pageCountPerBlock;

	if (dirtyList.size() == 0) { // dirtyList is empty, scan freeList
		if (freeList.size() == 0) { // freeList is empty, scan deadList
			if (deadList.size() == 0) { // deadList is empty, R U KIDDING ME?!!
				pba.blockID = pba.pageID = -1;
				return pba;	// impossible return~
			} //~ if (deadList.size() == 0)

			// reclaim dead blocks
			RV rv = ReclaimBlock();

			if (rv != RV_OK) { // garbage collection failed
				pba.blockID = pba.pageID = -1;
				return pba;
			}

			return AllocNewPage();	// re-allocate again
		} //~ if (freeList.size() == 0)

		// freeList is not empty...
		// allocate the first free block
		pba.blockID = freeList.front();
		pba.pageID = 0;	// the 1st page
		SetPageState(pba, ALLC);

		// move the block to dirty block list
		freeList.erase(freeList.begin());
		dirtyList.push_back(pba.blockID);

		return pba;
	} else { // dirtyList isn't empty, scan it
		BLOCK_ID _pos;

		for (_pos = 0; _pos < (int)dirtyList.size(); _pos++) {
			if (blockState[dirtyList[_pos]].freePages > 0) break;
		}

		if (_pos >= (int)dirtyList.size()) { // No page free in dirtyList (allc, live, dead), scan freeList
			if (freeList.size() == 0) { // no free page in freeList, active garbage collection
				RV rv = ReclaimBlock();

				if (rv != RV_OK) { // garbage collection failed
					pba.blockID = pba.pageID = -1;
					return pba;
				}

				return AllocNewPage();	// re-allocate again
			} else {
				pba.blockID = freeList.front();
				pba.pageID = 0;
				SetPageState(pba, ALLC);

				// move the block to dirty block list
				freeList.erase(freeList.begin());
				dirtyList.push_back(pba.blockID);

				return pba;
			}
		} else { // Select the unallocated page
			pba.blockID = dirtyList[_pos];

			for (int i = 0; i < pageCountPerBlock; i++) {
				if (GetPageState(PBA(pba.blockID, i)) == FREE) {
					pba.pageID = i;
					SetPageState(pba, ALLC);
					
					return pba;
				}
			}

			// no free page? It's impossible!
			pba.blockID = pba.pageID = -1;
			return pba;	// impossible return~~~~
		}
	}
}

/* Reclaim a block */
RV FTL01::ReclaimBlock(void)
{
	BLOCK_ID mostDirtyBlockID, leastEraseBlockID;
	int mostDirtyCount = -1, leastEraseCount = INT_MAX;	
	vector<BLOCK_ID>::iterator mostDirtyBlockIterator, leastEraseBlockIterator;

	int pageSize = flashDevice->GetModuleInfo().pageSize;
	int pageCountPerBlock = flashDevice->GetModuleInfo().pageCountPerBlock;
	BYTE * buffer = NULL;
	
	IVFD_COUNTER * pCounter = NULL;
	flashDevice->QueryInterface(IID_IVFD_COUNTER, (void **)&pCounter);

	if (!pCounter) { // Interface IVFD_COUNTER is not supported.
		return RV_ERROR_UNSUPPORT_OBJECT;
	}

	if (deadList.size() > 0) {
		// Erase blocks in deadList
		for (vector<BLOCK_ID>::iterator it = deadList.begin(); it != deadList.end(); ++it) {
			// erase all blocks
			flashDevice->EraseBlock(*it);
			for (int i = 0; i < pageCountPerBlock; i++)
				SetPageState(PBA(*it, i), FREE);

			freeList.push_back(*it);
		}

		// clear deadList
		deadList.clear();

		return RV_OK;
	}

	if (dirtyList.size() == 0) { // dirtyList is empty, no page can be reclaimed.
		return RV_ERROR_FLASH_NOT_DIRTY;
	}

	// Get the most dirty block and the least erase block from dirtyList
	for (vector<BLOCK_ID>::iterator it = dirtyList.begin(); it != dirtyList.end(); ++it) {
		if (blockState[*it].deadPages > mostDirtyCount) {
			mostDirtyCount = blockState[*it].deadPages;
			mostDirtyBlockID = *it;
			mostDirtyBlockIterator = it;
		}
		if (pCounter->GetEraseCount(*it) < leastEraseCount) {
			leastEraseCount = pCounter->GetEraseCount(*it);
			leastEraseBlockID = *it;
			leastEraseBlockIterator = it;
		}
	}

	if (mostDirtyCount <= 0) { // no block can be reclaimed
		return RV_ERROR_FLASH_NOT_DIRTY;
	}

	buffer = new BYTE[pageSize];

	// Wear leveling
	if (pCounter->GetEraseCount(mostDirtyBlockID) - leastEraseCount > info.wearLevelingThreshold) {
		// Just do it
		int index = 0;

		// Move live/allocated pages of leastEraseBlock to reserved block
		for (int i = 0; i < pageCountPerBlock; i++) {
			if (GetPageState(PBA(leastEraseBlockID, i)) == LIVE) {
				flashDevice->ReadPage(leastEraseBlockID, i, buffer, 0, pageSize);
				flashDevice->WritePage(reserved, index, buffer, 0, pageSize);
				SetPageState(PBA(reserved, index), LIVE);

				// Update LBA-PBA list
				LBA lba = reverseMapList[leastEraseBlockID * pageCountPerBlock + i];
				PBA pba(reserved, index);
				RegisterEntry(lba, pba);

				index++;
			} else if (GetPageState(PBA(leastEraseBlockID, i)) == ALLC) {
				SetPageState(PBA(reserved, index), ALLC);
				// Update LBA-PBA list
				LBA lba = reverseMapList[leastEraseBlockID * pageCountPerBlock + i];
				PBA pba(reserved, index);
				RegisterEntry(lba, pba);

				index++;
			}
		}

		// Erase leastEraseBlock block
		flashDevice->EraseBlock(leastEraseBlockID);

		for (int i = 0; i < pageCountPerBlock; i++) {
			SetPageState(PBA(leastEraseBlockID, i), FREE);
		}

		index = 0;

		// Move live pages of mostDirtyBlockID block to leaseEraseBlockID block
		for (int i = 0; i < pageCountPerBlock; i++) {
			if (GetPageState(PBA(mostDirtyBlockID, i)) == LIVE) {
				// move to reserved block
				flashDevice->ReadPage(mostDirtyBlockID, i, buffer, 0, pageSize);
				flashDevice->WritePage(leastEraseBlockID, index, buffer, 0, pageSize);
				SetPageState(PBA(leastEraseBlockID, index), LIVE);

				// Update LBA-PBA list
				LBA lba = reverseMapList[mostDirtyBlockID * pageCountPerBlock + i];
				PBA pba(leastEraseBlockID, index);
				RegisterEntry(lba, pba);

				index++;
			} else if (GetPageState(PBA(mostDirtyBlockID, i)) == ALLC) {
				SetPageState(PBA(leastEraseBlockID, index), ALLC);

				// Update LBA-PBA list
				LBA lba = reverseMapList[mostDirtyBlockID * pageCountPerBlock + i];
				PBA pba(leastEraseBlockID, index);
				RegisterEntry(lba, pba);

				index++;
			}
		}

		// Erase mostDirtyBlockID block
		flashDevice->EraseBlock(mostDirtyBlockID);
		for (int i = 0; i < pageCountPerBlock; i++) {
			SetPageState(PBA(mostDirtyBlockID, i), FREE);
		}

		// Remove mostDirtyBlockID block and leastEraseBlockID block from dirtyList
		dirtyList.erase(mostDirtyBlockIterator);

		// Add reserved block and leastEraseBlockID block into dirtyList
		dirtyList.push_back(reserved);

		reserved = mostDirtyBlockID;
	} else {
		int index = 0;

		// Move live/allocated pages of mostDirtyBlockID block to reserved block
		for (int i = 0; i < pageCountPerBlock; i++) {
			if (GetPageState(PBA(mostDirtyBlockID, i)) == LIVE) {
				// Move live pages to reserved block
				flashDevice->ReadPage(mostDirtyBlockID, i, buffer, 0, pageSize);
				flashDevice->WritePage(reserved, index, buffer, 0, pageSize);
				SetPageState(PBA(reserved, index), LIVE);

				// Update LBA-PBA list
				LBA lba = reverseMapList[mostDirtyBlockID * pageCountPerBlock + i];
				PBA pba(reserved, index);
				RegisterEntry(lba, pba);

				index++;
			} else if (GetPageState(PBA(mostDirtyBlockID, i)) == ALLC) {
				SetPageState(PBA(reserved, index), ALLC);

				// Update LBA-PBA list
				LBA lba = reverseMapList[mostDirtyBlockID * pageCountPerBlock + i];
				PBA pba(reserved, index);
				RegisterEntry(lba, pba);

				index++;
			}
		}

		// Erase mostDirtyBlockID block
		flashDevice->EraseBlock(mostDirtyBlockID);
		for (int i = 0; i < pageCountPerBlock; i++) {
			SetPageState(PBA(mostDirtyBlockID, i), FREE);
		}

		// Remove mostDirtyBlockID block from dirtyList
		dirtyList.erase(mostDirtyBlockIterator);

		// Add reserved block into dirtyList
		dirtyList.push_back(reserved);

		// Make erased block as reserved block
		reserved = mostDirtyBlockID;
	}

	delete [] buffer;

	return RV_OK;
}

/* Get the data state of specified page */
PAGE_STATE FTL01::GetPageState(PBA pba)
{
	ASSERT(blockState);

	return blockState[pba.blockID].pageStates[pba.pageID];
}

/* Set the data state of specified page */
void FTL01::SetPageState(PBA pba,
						 PAGE_STATE ps)
{
	ASSERT(blockState);
	
	PAGE_STATE _originalPS = GetPageState(pba);

	if (_originalPS == ps) return;

	switch (_originalPS) {
		case FREE:
			blockState[pba.blockID].freePages --;
			break;
		case ALLC:
			blockState[pba.blockID].allocatedPages --;
			break;
		case LIVE:
			blockState[pba.blockID].livePages --;
			break;
		case DEAD:
			blockState[pba.blockID].deadPages --;
			break;
	}
	switch (ps) {
		case FREE:
			blockState[pba.blockID].freePages ++;
			break;
		case ALLC:
			blockState[pba.blockID].allocatedPages ++;
			break;
		case LIVE:
			blockState[pba.blockID].livePages ++;
			break;
		case DEAD:
			blockState[pba.blockID].deadPages ++;
			if (blockState[pba.blockID].deadPages == flashDevice->GetModuleInfo().pageCountPerBlock) {
				// all pages in the block are dead			
				MoveDirtyToDead(pba.blockID);
			}
			break;
	}

	blockState[pba.blockID].pageStates[pba.pageID] = ps;
}

/* Move a dirty block from dirtyList to deadList */
void FTL01::MoveDirtyToDead(BLOCK_ID blockID)
{
	for (vector<BLOCK_ID>::iterator it = dirtyList.begin(); it != dirtyList.end(); ++it) {
		if (*it == blockID) {
			dirtyList.erase(it);	// remove from dirtyList
			break;
		}
	}

	deadList.push_back(blockID);
}
