// FlashDBSimDll.cpp : ¶¨Òå DLL Ó¦ÓÃ³ÌÐòµÄµ¼³öº¯Êý¡£
//
#include "../../SO/include/FlashDBSimSO.h"
#include "FlashDBSim_capi.h"

extern "C" {
	
	FTL_INFO_t * create_ftl_info(IDM_FTL id, int mapListSize, int wearLevelingThreshold){

		FTL_INFO *ftlInfo = new FTL_INFO(id, mapListSize, wearLevelingThreshold);
	
		return (FTL_INFO_t *)ftlInfo;
	}
	
	void check_ftl_info(FTL_INFO_t* ftlInfo){
		printf("FTL_INFO\n");
		printf("IDM_FTL id:            %d\n", (ftlInfo->id));
		printf("mapListSize:           %d\n", (ftlInfo->mapListSize));
		printf("wearLevelingThreshold: %d\n\n", (ftlInfo->wearLevelingThreshold));
	}
	
	VFD_INFO_t * create_vfd_info(IDM_VFD id, int blockCount, int pageCountPerBlock, int pageSize1, int pageSize2, int eraseLimitation, int readRandomTime, int readSerialTime, int programTime, int eraseTime){
		
		VFD_INFO *vfdInfo = new VFD_INFO();
		
		vfdInfo->id = id;
		vfdInfo->blockCount = blockCount;
		vfdInfo->pageCountPerBlock = pageCountPerBlock;
		vfdInfo->pageSize.size1 = pageSize1;
		vfdInfo->pageSize.size2 = pageSize2;
		vfdInfo->eraseLimitation = eraseTime;
		vfdInfo->readTime.randomTime = readRandomTime;
		vfdInfo->readTime.serialTime = readSerialTime;
		vfdInfo->programTime = programTime;
		vfdInfo->eraseTime = eraseTime;
		
		//~ VFD_INFO *vfdInfo = new VFD_INFO(id, blockCount, pageSize1, pageSize2, pageCountPerBlock, eraseLimitation, readRandomTime, readSerialTime, programTime, eraseTime);
		
		return (VFD_INFO_t *) vfdInfo;
	}
	
	void check_vfd_info(VFD_INFO_t* vfdInfo){
		printf("VFD_INFO\n");
		printf("IDM_VFD id:        %d\n", vfdInfo->id);
		printf("blockCount:        %d\n", vfdInfo->blockCount);
		printf("pageCountPerBlock: %d\n", vfdInfo->pageCountPerBlock);
		printf("pageSize1:         %d\n", vfdInfo->pageSize.size1);
		printf("pageSize2:         %d\n", vfdInfo->pageSize.size2);
		printf("eraseLimitation:   %d\n", vfdInfo->eraseLimitation);
		printf("readrandomTime:    %d\n", vfdInfo->readTime.randomTime);
		printf("readserialTime:    %d\n", vfdInfo->readTime.serialTime);
		printf("programTime:       %d\n", vfdInfo->programTime);
		printf("eraseTime:         %d\n\n", vfdInfo->eraseTime);
	}
	
	RV f_initialize_c(const VFD_INFO_t* vfdInfo, const FTL_INFO_t* ftlInfo) {
		return FlashDBSim::Initialize(*vfdInfo, *ftlInfo);
	}

	RV f_release_c(void) {
		return FlashDBSim::Release();
	}

	int f_alloc_page_c(int count, LBA * lbas) {
		return FlashDBSim::AllocPage(count, lbas);
	}

	RV f_release_page_c(LBA lba) {
		return FlashDBSim::ReleasePage(lba);
	}

	RV f_read_page_c(LBA lba, BYTE * buffer, int offset, size_t size) {
		return FlashDBSim::ReadPage(lba, buffer, offset, size);
	}

	RV f_write_page_c(LBA lba, const BYTE * buffer, int offset, size_t size) {
		return FlashDBSim::WritePage(lba, buffer, offset, size);
	}
	
	IVFD_COUNTER_t f_get_vfd_counter_c(void){
		IVFD * vfd = const_cast<IVFD *>(f_get_vfd_module()); /*pegando o disposito flash*/
		IVFD_COUNTER * icounter = NULL;
		vfd->QueryInterface(IID_IVFD_COUNTER, (void**)&icounter);
		return icounter;
	}
	
	int f_get_read_count_total_c(IVFD_COUNTER_t icounter){
		return ((IVFD_COUNTER*)icounter)->GetReadCountTotal();
	}
	
	int f_get_write_count_total_c(IVFD_COUNTER_t icounter){
		return ((IVFD_COUNTER*)icounter)->GetWriteCountTotal();
	}
	
	int f_get_erase_count_total_c(IVFD_COUNTER_t icounter){
		return ((IVFD_COUNTER*)icounter)->GetEraseCountTotal();
	}
	
	int f_get_read_count_c(IVFD_COUNTER_t icounter, BLOCK_ID blockID, PAGE_ID pageID){
		return ((IVFD_COUNTER*)icounter)->GetReadCount(blockID, pageID);
	}
	
	int f_get_write_count_c(IVFD_COUNTER_t icounter, BLOCK_ID blockID, PAGE_ID pageID){
		return ((IVFD_COUNTER*)icounter)->GetWriteCount(blockID, pageID);
	}
	
	int f_get_erase_count_c(IVFD_COUNTER_t icounter, BLOCK_ID blockID){
		return ((IVFD_COUNTER*)icounter)->GetEraseCount(blockID);
	}

	void f_reset_read_count_c(IVFD_COUNTER_t icounter){
		((IVFD_COUNTER*)icounter)->ResetReadCount();
	}
	
	void f_reset_write_count_c(IVFD_COUNTER_t icounter){
		((IVFD_COUNTER*)icounter)->ResetWriteCount();
	}
	
	void f_reset_erase_count_c(IVFD_COUNTER_t icounter){
		((IVFD_COUNTER*)icounter)->ResetEraseCount();
	}
	
	void f_reset_counter_c(IVFD_COUNTER_t icounter){
		((IVFD_COUNTER*)icounter)->ResetCounter();
	}
	
	IVFD_LATENCY_t f_get_vfd_latency_c(void){
		IVFD * vfd = const_cast<IVFD *>(f_get_vfd_module()); /*pegando o disposito flash*/
		IVFD_LATENCY * ilatency = NULL;
		vfd->QueryInterface(IID_IVFD_LATENCY, (void**)&ilatency);
		return ilatency;
	}
	
	int f_get_read_latency_total_c(IVFD_LATENCY_t ilatency){
		return ((IVFD_LATENCY*)ilatency)->GetReadLatencyTotal();
	}
	
	int f_get_write_latency_total_c(IVFD_LATENCY_t ilatency){
		return ((IVFD_LATENCY*)ilatency)->GetWriteLatencyTotal();
	}
	
	int f_get_erase_latency_total_c(IVFD_LATENCY_t ilatency){
		return ((IVFD_LATENCY*)ilatency)->GetEraseLatencyTotal();
	}

	void f_reset_read_latency_total_c(IVFD_LATENCY_t ilatency){
		((IVFD_LATENCY*)ilatency)->ResetReadLatencyTotal();
	}
	
	void f_reset_write_latency_total_c(IVFD_LATENCY_t ilatency){
		((IVFD_LATENCY*)ilatency)->ResetWriteLatencyTotal();
	}
	
	void f_reset_erase_latency_total_c(IVFD_LATENCY_t ilatency){
		((IVFD_LATENCY*)ilatency)->ResetEraseLatencyTotal();
	}
	
	void f_reset_latency_total_c(IVFD_LATENCY_t ilatency){
		((IVFD_LATENCY*)ilatency)->ResetLatencyTotal();
	}
	
	/*FlashDBSim nao coleta informacoes estatisticas do FTL
	 * numero de gc feitos
	 * TO DO: implementar uma interface para isso
	 */
}


