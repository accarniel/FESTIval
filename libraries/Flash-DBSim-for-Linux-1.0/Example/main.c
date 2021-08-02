#include "FlashDBSim_capi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FRAMESIZE 2048 /*corresponde ao tamanho da pagina na memoria flash*/
#define TRUE 1
#define DEBUG 1

int main(){

	FTL_INFO_t * ftlInfo = NULL;
	//~ FTL_INFO_t * create_ftl_info(int id, int mapListSize, int wearLevelingThreshold);
	ftlInfo = create_ftl_info(ID_FTL_01_t, 65536, 4);
	
	VFD_INFO_t * vfdInfo = NULL;
	//~ VFD_INFO_t * create_vfd_info(int id, int blockCount, int pageCountPerBlock, int pageSize1, int pageSize2, int eraseLimitation, int readRandomTime, int readSerialTime, int programTime, int eraseTime);
	vfdInfo = create_vfd_info(ID_NAND_DEVICE_03_t, 1024, 64, 2048 /*tamanho da pagina*/, 0, 100000, 25, 0, 200, 1500);
	
	if(DEBUG == TRUE){
		check_ftl_info(ftlInfo);
		check_vfd_info(vfdInfo);
	}
		
	int rv;
	
	if(DEBUG == TRUE){
		printf("Initializing FlashDBSim ...\n\n");
	}
	
	rv = f_initialize_c(vfdInfo, ftlInfo);
	if (rv == RV_FAIL)
	{	
		printf("Failed to start FlashDBSim\n");
		return -1;
	}
	
	if(DEBUG == TRUE){
		printf("FlashDBSim initialized!!!\n\n");
	}
	
	int pid = -1, pid2 = -1;
	
	if(DEBUG == TRUE){
		printf("Allocating a page ...\n\n");
	}
	
	//alocando uma pagina
	f_alloc_page_c(1, &pid);
	if (pid == -1)
	{	
		printf("Failed to allocate page\n");
		printf("There is no free page in the flash memory!\n");
	}
	
	if(DEBUG == TRUE){
		printf("Page allocated with PID: %d\n\n", pid);
	}
	
	if(DEBUG == TRUE){
		printf("Allocating a page ...\n\n");
	}
	
	//alocando uma pagina
	f_alloc_page_c(1, &pid2);
	if (pid2 == -1)
	{	
		printf("Failed to allocate page\n");
		printf("There is no free page in the flash memory!\n");
	}
	
	if(DEBUG == TRUE)
		printf("Page allocated with PID: %d\n\n", pid2);
	
	//usado para pegar estatisticas do vfd
	IVFD_COUNTER_t ic = f_get_vfd_counter_c();
	IVFD_LATENCY_t il = f_get_vfd_latency_c();
	
	if(DEBUG == TRUE){
		printf("Read Count Total is:    %d\n", f_get_read_count_total_c(ic));
		printf("Write Count Total is:   %d\n", f_get_write_count_total_c(ic));
		printf("Erase Count Total is:   %d\n", f_get_erase_count_total_c(ic));
	
		printf("Read Latency Total is:  %d\n", f_get_read_latency_total_c(il));// /25);
		printf("Write Latency Total is: %d\n", (f_get_write_latency_total_c(il)));//-10001*200)/200);
		printf("Erase Latency Total is: %d\n", f_get_erase_latency_total_c(il));// /1500);
		printf("Total Latency is:       %d\n\n", f_get_read_latency_total_c(il)+f_get_write_latency_total_c(il)+f_get_erase_latency_total_c(il));//-10001*220);
	}
		
	BYTE *buf1 = (BYTE *) malloc(sizeof(BYTE)*FRAMESIZE);
	BYTE *buf2 = (BYTE *) malloc(sizeof(BYTE)*FRAMESIZE);
	BYTE *buf3 = (BYTE *) malloc(sizeof(BYTE)*FRAMESIZE);
	BYTE *buf4 = (BYTE *) malloc(sizeof(BYTE)*FRAMESIZE);
	
	strcpy(buf1, "teste1");
	strcpy(buf2, "teste2");
	
	if(DEBUG == TRUE){
		printf("Writting a page ...\n\n");
	}
	
	//escrevendo uma pagina
	//~ RV f_write_page_c(LBA_t lba, const BYTE_t * buffer, int offset, size_t size);
	rv = f_write_page_c(pid,buf1,0,FRAMESIZE);
	if (rv == RV_ERROR_FLASH_NO_MEMORY)
	{	
		printf("There is no space in the flash memory!\n");
	}
	if (rv == RV_OK){
		if(DEBUG == TRUE){
			printf("Page written!!!\n\n");
		}
	}
	
	if(DEBUG == TRUE){
		printf("Read Count Total is:    %d\n", f_get_read_count_total_c(ic));
		printf("Write Count Total is:   %d\n", f_get_write_count_total_c(ic));
		printf("Erase Count Total is:   %d\n", f_get_erase_count_total_c(ic));
	
		printf("Read Latency Total is:  %d\n", f_get_read_latency_total_c(il));// /25);
		printf("Write Latency Total is: %d\n", (f_get_write_latency_total_c(il)));//-10001*200)/200);
		printf("Erase Latency Total is: %d\n", f_get_erase_latency_total_c(il));// /1500);
		printf("Total Latency is:       %d\n\n", f_get_read_latency_total_c(il)+f_get_write_latency_total_c(il)+f_get_erase_latency_total_c(il));//-10001*220);
	}
	
	if(DEBUG == TRUE){
		printf("Writting a page ...\n\n");
	}
	
	//escrevendo uma pagina
	//~ RV f_write_page_c(LBA_t lba, const BYTE_t * buffer, int offset, size_t size);
	rv = f_write_page_c(pid2,buf2,0,FRAMESIZE);
	if (rv == RV_ERROR_FLASH_NO_MEMORY)
	{	
		printf("There is no space in the flash memory!\n");
	}
	if (rv == RV_OK){
		if(DEBUG == TRUE){
			printf("Page written!!!\n\n");
		}
	}
	
	if(DEBUG == TRUE){
		printf("Read Count Total is:    %d\n", f_get_read_count_total_c(ic));
		printf("Write Count Total is:   %d\n", f_get_write_count_total_c(ic));
		printf("Erase Count Total is:   %d\n", f_get_erase_count_total_c(ic));
	
		printf("Read Latency Total is:  %d\n", f_get_read_latency_total_c(il));// /25);
		printf("Write Latency Total is: %d\n", (f_get_write_latency_total_c(il)));//-10001*200)/200);
		printf("Erase Latency Total is: %d\n", f_get_erase_latency_total_c(il));// /1500);
		printf("Total Latency is:       %d\n\n", f_get_read_latency_total_c(il)+f_get_write_latency_total_c(il)+f_get_erase_latency_total_c(il));//-10001*220);
	}
	
	if(DEBUG == TRUE){
		printf("Reading a page ...\n\n");
	}
	
	//lendo uma pagina
	rv = f_read_page_c(pid,buf3,0,FRAMESIZE);
	if(rv == RV_ERROR_INVALID_PAGE_STATE)
	{
		printf("page readed is invalid\n");
	}
	if (rv ==  RV_ERROR_FLASH_BLOCK_BROKEN)
	{
		printf("the block contained this page is broken \n");
	}
	if (rv == RV_OK){
		if(DEBUG == TRUE){
			printf("Page read: %s\n\n", buf3);
		}
	}
		
	if(DEBUG == TRUE){
		printf("Read Count Total is:    %d\n", f_get_read_count_total_c(ic));
		printf("Write Count Total is:   %d\n", f_get_write_count_total_c(ic));
		printf("Erase Count Total is:   %d\n", f_get_erase_count_total_c(ic));
	
		printf("Read Latency Total is:  %d\n", f_get_read_latency_total_c(il));// /25);
		printf("Write Latency Total is: %d\n", (f_get_write_latency_total_c(il)));//-10001*200)/200);
		printf("Erase Latency Total is: %d\n", f_get_erase_latency_total_c(il));// /1500);
		printf("Total Latency is:       %d\n\n", f_get_read_latency_total_c(il)+f_get_write_latency_total_c(il)+f_get_erase_latency_total_c(il));//-10001*220);
	}
	
	if(DEBUG == TRUE){
		printf("Reading a page ...\n\n");
	}
	
	//lendo uma pagina
	rv = f_read_page_c(pid2,buf4,0,FRAMESIZE);
	if(rv == RV_ERROR_INVALID_PAGE_STATE)
	{
		printf("page readed is invalid\n");
	}
	if (rv ==  RV_ERROR_FLASH_BLOCK_BROKEN)
	{
		printf("the block contained this page is broken \n");
	}
	if (rv == RV_OK){
		if(DEBUG == TRUE){
			printf("Page read: %s\n\n", buf4);
		}
	}
	
	//mostrando estatisticas
	printf("Read Count Total is:    %d\n", f_get_read_count_total_c(ic));
	printf("Write Count Total is:   %d\n", f_get_write_count_total_c(ic));
	printf("Erase Count Total is:   %d\n", f_get_erase_count_total_c(ic));

	printf("Read Latency Total is:  %d\n", f_get_read_latency_total_c(il));// /25);
	printf("Write Latency Total is: %d\n", (f_get_write_latency_total_c(il)));//-10001*200)/200);
	printf("Erase Latency Total is: %d\n", f_get_erase_latency_total_c(il));// /1500);
	printf("Total Latency is:       %d\n\n", f_get_read_latency_total_c(il)+f_get_write_latency_total_c(il)+f_get_erase_latency_total_c(il));//-10001*220);
	
	if(DEBUG == TRUE){
		printf("Releasing page with PID: %d\n\n", pid);
	}
	
	//desalocando pagina	
	rv = f_release_page_c(pid);
	if (rv == RV_ERROR_INVALID_LBA)
	{	
		printf("Invalid  LBA\n");
	}
	if (rv == RV_OK){
		if(DEBUG == TRUE){
			printf("Page released!!! \n\n");
		}
	}
	
	if(DEBUG == TRUE){
		printf("Releasing page with PID: %d\n\n", pid2);
	}
	
	//desalocando pagina
	rv = f_release_page_c(pid2);
	if (rv == RV_ERROR_INVALID_LBA)
	{	
		printf("Invalid  LBA\n");
	}
	if (rv == RV_OK){
		if(DEBUG == TRUE){
			printf("Page released!!! \n\n");
		}
	}
	
	if(DEBUG == TRUE){
		printf("Shutting down FlashDBSim ...\n\n");
	}
	
	//desalocando flashdbsim
	rv = f_release_c();
	if (rv == RV_FAIL)
	{	
		printf("Failed to finalize FlashDBSim!\n");
	}
	
	if(DEBUG == TRUE){
		printf("FlashDBSim finalized!!!\n\n");
	}
	
	free(buf1);
	free(buf2);
	free(buf3);
	free(buf4);
	
	return 0;
}
