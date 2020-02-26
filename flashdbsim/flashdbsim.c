/**********************************************************************
 *
 * FESTIval - Framework to Evaluate SpaTial Indices in non-VolAtiLe memories and hard disk drives.
 * https://accarniel.github.io/FESTIval/
 *
 * Copyright (C) 2016-2020 Anderson Chaves Carniel <accarniel@gmail.com>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU General Public Licence. See the COPYING file.
 *
 * Developed by Anderson Chaves Carniel
 * Partially developed by Kairo Bonicenha
 *
 **********************************************************************/

#include <liblwgeom.h>

#include "flashdbsim.h"
#include "FlashDBSim_capi.h" //for the flashdbsim functions implemented by Tamires Brito da Silva (2017)

#include "../libraries/uthash/uthash.h"
#include "../main/log_messages.h"

#include "../main/io_handler.h" /*because of the type of memory allocations (direct or normal) */

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)

#undef uthash_fatal
#define uthash_fatal(msg) _DEBUG(ERROR, msg)

/*These structures below are HASH tables used for mapping the location of 
 * idx_pages to the pages of the simulator (because the application can write
 * non-sequential index pages)*/

/* In this case, we do this mapping when the flash page has a SMALLER size than the index page
 * e.g.: idx_page = 5 can be stored in the following flash pages: 10, 11, 12, 13
 */
typedef struct MapLowFlashPage {
    UT_hash_handle hh;

    int idx_page; //the key
    int flash_page;
} MapLowFlashPage;

/* In this case, we do this mapping when the flash page has a GREATER size than the index page
 * e.g.: idx_page = 5 can be stored in the following flash pages: 1
 * at the same time, the flash page can store the idx_pages: 5, 7, 9, 10
 * thus, we need to know the position of the idx_page in such flash page (flash_offset) 
 */
typedef struct MapHighFlashPage {
    UT_hash_handle hh;

    int idx_page; //the key
    int flash_offset; //the position of the idx_page in the flash_page
    int flash_page;
} MapHighFlashPage;

static MapLowFlashPage *mlfp = NULL;
static MapHighFlashPage *mhfp = NULL;

/*to handle a specific case of maphighflashpage we these following variables and structs*/
static int lastAllocatedPage = -1;
static int lastOffset = -1;

/*this hash map stores the flash pages of the maphighflashpage that have 'holes'*/
typedef struct FlashPagesWithSpace {
    UT_hash_handle hh;

    int flash_page; //the key
    int *positions; //the positions of the flash page that have space to store
    int n; //the number of available positions
} FlashPagesWithSpace;
static FlashPagesWithSpace *removedIndexPages = NULL;

/*
 Initialized the flash simulator
 */
void flashdbsim_initialize(const FlashDBSim *si) {
    int rv;
    //FTL definition
    FTL_INFO_t * ftlInfo = NULL;
    //Virtual Flash Device definition
    VFD_INFO_t * vfdInfo = NULL;   

    ftlInfo = create_ftl_info(si->ftl_type, si->map_list_size,
            si->wear_leveling_threshold);

    vfdInfo = create_vfd_info(si->nand_device_type, si->block_count,
            si->page_count_per_block, si->page_size1, si->page_size2,
            si->erase_limitation, si->read_random_time, si->read_serial_time,
            si->program_time, si->erase_time);
    //the initialization occurs here
    rv = f_initialize_c(vfdInfo, ftlInfo);
    if (rv == RV_FAIL) {
        _DEBUG(ERROR, "Failed to start FlashDBSim");
    }
}

/*free all the memory allocated by the flashdbsim*/
void flashdbsim_release(void) {
    int rv;
    rv = f_release_c();
    if (rv == RV_FAIL) {
        _DEBUG(ERROR, "Failed to finalize FlashDBSim!");
    }
}

/*developed by Kairo Bonicenha and commented by Anderson Chaves Carniel May, 2017*/
void flashdbsim_read_one_page(const SpatialIndex *si, int idx_page, uint8_t *buf) {
    int rv; //debugger
    /*this is a number to deal with the following cases:
        (i) the page size of simulator is GREATER than the page size of index
              prop = amount of pages of index that will fit on the page of
              simulator
        (ii) the page of simulator is SMALLER than the page size of index
              prop = number of pages of simulator needed to store 
              an index page
     *  (iii) the page of simulator is equal to the page size of index
     *        prop is equal to 1     
     */
    int prop;
    FlashDBSim *simulator = (FlashDBSim*) si->gp->storage_system->info;

    //_DEBUGF(NOTICE, "reading the index page %d", idx_page);

    //Case (i): flash page is SMALLER than a page of index 
    if (simulator->page_size1 < si->gp->page_size) {
        int i;
        uint8_t *buf_temp; /*store a part of the node*/
        MapLowFlashPage *entrymlfp; //entry of the mapping

        prop = si->gp->page_size / simulator->page_size1;

        //_DEBUGF(NOTICE, "it is the case (i) -> index page size %d and simulator page size %d",
        //        si->gp->page_size, simulator->page_size1);

        //get the mapping value
        HASH_FIND_INT(mlfp, &idx_page, entrymlfp);
        if (entrymlfp == NULL) {
            _DEBUGF(ERROR, "Node (%d) do not found in the mapping of the "
                    "FlashDBSim simulator.", idx_page);
        }

        if (si->gp->io_access == DIRECT_ACCESS) {
            //then the memory must be aligned in blocks!
            if (posix_memalign((void**) &buf_temp, simulator->page_size1, simulator->page_size1)) {
                _DEBUG(ERROR, "Allocation failed at get_rnode");
            }
        } else {
            buf_temp = (uint8_t*) lwalloc(simulator->page_size1);
        }

        /*get all parts of the node
         i.e., we traverse all the flash pages that stores this index page*/
        for (i = 0; i < prop; i++) {
            //read page
            rv = f_read_page_c(entrymlfp->flash_page + i, buf_temp, 0,
                    simulator->page_size1);

            if (rv == RV_ERROR_INVALID_PAGE_STATE) {
                _DEBUG(ERROR, "FlashDBSim: page read is invalid");
            }
            if (rv == RV_ERROR_FLASH_BLOCK_BROKEN) {
                _DEBUG(ERROR, "FlashDBSim: the block contained the read page is broken");
            }
            if (rv == RV_OK) {
                /*if read is ok, save the page in the buffer which will contain
                 all the parts of the pages of simulator
                 */
                memcpy(buf + i * simulator->page_size1, buf_temp,
                        simulator->page_size1);
            } else {
                _DEBUGF(ERROR, "FlashDBSim has reported an unknown error: %d", rv);
            }
        }
        
        if (si->gp->io_access == DIRECT_ACCESS) {
            free(buf_temp);
        } else {
            lwfree(buf_temp);
        }
    } else
        //Case (ii): flash page is GREATER than a page of index 
        if (simulator->page_size1 > si->gp->page_size) {
        MapHighFlashPage *entrymhfp; //entry of the mapping

        prop = simulator->page_size1 / si->gp->page_size;

        //_DEBUGF(NOTICE, "it is the case (ii) -> index page size %d and simulator page size %d",
        //        si->gp->page_size, simulator->page_size1);

        //get the mapping value
        HASH_FIND_INT(mhfp, &idx_page, entrymhfp);
        if (entrymhfp == NULL) {
            _DEBUGF(ERROR, "Node (%d) do not found in the mapping of the "
                    "FlashDBSim simulator.", idx_page);
        }
        /*read the part of page of simulator which contain the 
         all page of index*/
        rv = f_read_page_c(entrymhfp->flash_page, buf,
                entrymhfp->flash_offset * si->gp->page_size, si->gp->page_size);
        if (rv == RV_ERROR_INVALID_PAGE_STATE) {
            _DEBUG(ERROR, "FlashDBSim: page read is invalid");
        }
        if (rv == RV_ERROR_FLASH_BLOCK_BROKEN) {
            _DEBUG(ERROR, "FlashDBSim: the block contained the read page is broken");
        }

    } else {
        //Otherwise, the flash page size is EQUAL to the page size of the index 
        MapLowFlashPage *entrymlfp; //structure of mapping

        //_DEBUGF(NOTICE, "it is the case (iii) -> index page size %d and simulator page size %d",
        //        si->gp->page_size, simulator->page_size1);

        //get the mapping value
        HASH_FIND_INT(mlfp, &idx_page, entrymlfp);
        if (entrymlfp == NULL) {
            _DEBUGF(ERROR, "Node (%d) do not found in the mapping of the "
                    "FlashDBSim simulator.", idx_page);
        }
        //read the page from the simulator
        rv = f_read_page_c(entrymlfp->flash_page, buf, 0, si->gp->page_size);
        if (rv == RV_ERROR_INVALID_PAGE_STATE) {
            _DEBUG(ERROR, "FlashDBSim: page read is invalid");
        }
        if (rv == RV_ERROR_FLASH_BLOCK_BROKEN) {
            _DEBUG(ERROR, "FlashDBSim: the block contained the read page is broken");
        }
    }
    //_DEBUG(NOTICE, "flashdbsim read page done");
}

/*there are three cases for write a page:
 (i) a newly created page,
 (ii) the update operation of an existing page, and
 (iii) the deletion of a page
 to handle these cases, we make use of the mapping structures defined above*/
void flashdbsim_write_one_page(const SpatialIndex *si, uint8_t *buf, int idx_page) {
    int32_t checker; //check if it a deletion operation
    /* there are three cases to handle pages here*/
    FlashDBSim *simulator = (FlashDBSim*) si->gp->storage_system->info;
    int rv; //the returned value of a flashdbsim operation

    /*to know if it is a deletion case 
     * we read the first int of the buffer and see if its value is equal to -1 (see rnode.c file)
     * */
    memcpy(&checker, buf, sizeof (int32_t));

    //_DEBUGF(NOTICE, "writing the index page %d", idx_page);

    /*First case: the flash simulator has a page size SMALLER than the page size of the index
     for this case, we use the following mapping: MapLowFlashPage*/
    if (simulator->page_size1 < si->gp->page_size) {
        /*in this case we have to alloc N pages of the flash simulator 
         in order to store a node of the index
         both sizes are power 2!
         Example:   flash simulator has a page size equal to 512bytes
                    index employ a page size equal to 4KB
         in this case, we have to alloc 8 pages in the simulator to store each node of the index*/
        int n; //the number of flash pages needed to store a node
        int i;
        LBA *pids; //the ids of the allocated flash pages
        MapLowFlashPage *entry;

        n = si->gp->page_size / simulator->page_size1;

        //_DEBUGF(NOTICE, "it is the case (i) -> index page size %d and simulator page size %d",
        //        si->gp->page_size, simulator->page_size1);

        //we only alloc pages if this page is a newly created page in the flash
        HASH_FIND_INT(mlfp, &idx_page, entry);
        if (entry == NULL) {
            pids = (LBA*) lwalloc(n * sizeof (LBA));
            f_alloc_page_c(n, pids);

            /*check possible errors*/
            for (i = 0; i < n; i++) {
                if (pids[i] == -1) {
                    _DEBUG(ERROR, "FlashDBSim: Failed to allocate page."
                            "There is no free page in the flash memory!");
                }
            }

            //in addition, we store it in our mapping
            entry = (MapLowFlashPage*) lwalloc(sizeof (MapLowFlashPage));
            entry->flash_page = pids[0]; //this is the first flash page
            entry->idx_page = idx_page;
            HASH_ADD_INT(mlfp, idx_page, entry);
        }

        //note that we don't alloc new pages for the update or deletion case

        if (checker == -1) {
            //this is a remotion operation and thus, we have to dealloc the pages!            
            for (i = 0; i < n; i++) {
                f_release_page_c(entry->flash_page + i);
            }
            //in addition, we have to remove the entries of the mapping
            HASH_DEL(mlfp, entry);
            lwfree(entry);
        } else {
            //this is an update or creation case: both cases we have to write in the flash pages
            uint8_t *buf_temp;
            int offset = 0; //this is used to help to store parts of the node
            
            if (si->gp->io_access == DIRECT_ACCESS) {
                //then the memory must be aligned in blocks!
                if (posix_memalign((void**) &buf_temp, simulator->page_size1, simulator->page_size1)) {
                    _DEBUG(ERROR, "Allocation failed at get_rnode");
                }
            } else {
                buf_temp = (uint8_t*) lwalloc(simulator->page_size1);
            }

            
            for (i = 0; i < n; i++) {
                memcpy(buf_temp, buf + offset, simulator->page_size1);
                rv = f_write_page_c(entry->flash_page + i, buf_temp, 0, simulator->page_size1);
                if (rv == RV_ERROR_FLASH_NO_MEMORY) {
                    _DEBUG(ERROR, "FlashDBSim: There is no space in the flash memory!");
                }
                offset += simulator->page_size1;
            }

            if (si->gp->io_access == DIRECT_ACCESS) {
                free(buf_temp);
            } else {
                lwfree(buf_temp);
            }
        }
    } else
        /*Second case: the flash simulator has a page size GREATER than the page size of the index
         * for this case, we use the following mapping: MapHighFlashPage*/
        if (simulator->page_size1 > si->gp->page_size) {
        /*in this case we have to alloc 1 flash page that will store N index pages 
     both sizes are power 2!
     Example:   flash simulator has a page size equal to 4KB
                index employ a page size equal to 512bytes
     in this case, we can store 8 nodes in a same flash page*/
        int n; //the number of index pages can a flash page can hold
        LBA pid; //the id of the allocated flash page
        MapHighFlashPage *entry;
        bool deadpage = false; //the page was deallocated?

        n = simulator->page_size1 / si->gp->page_size;

        //_DEBUGF(NOTICE, "it is the case (ii) -> index page size %d and simulator page size %d",
        //        si->gp->page_size, simulator->page_size1);

        HASH_FIND_INT(mhfp, &idx_page, entry);
        if (entry == NULL) {
            int pos = -1; //what is the position to put the new index page?
            int fpage = -1; //what is the flash page to write the new index page?

            /*this page was not previously stored in the flash
             *then we check in our list for flash pages with some space*/
            if (HASH_COUNT(removedIndexPages) > 0) {
                FlashPagesWithSpace *avaiPage, *temp;

                HASH_ITER(hh, removedIndexPages, avaiPage, temp) {
                    if (avaiPage->positions != NULL && avaiPage->n > 0) {
                        pos = avaiPage->positions[avaiPage->n - 1];
                        fpage = avaiPage->flash_page;

                        avaiPage->n--;

                        if (pos > n) {
                            _DEBUGF(ERROR, "We tried to reutilize the space of "
                                    "the flash page %d in the position %d,"
                                    " but this position is higher than "
                                    "the number of index pages (%d)"
                                    " stored in the flash page",
                                    fpage, pos, n);
                        }

                        //therefore, we remove this entry
                        if (avaiPage->n <= 0) {
                            HASH_DEL(removedIndexPages, avaiPage);
                            lwfree(avaiPage->positions);
                            lwfree(avaiPage);
                        }
                        break;
                    }
                }
            }/*second case: we check if the last allocated page in the flash has enough space
             *to store other index node*/
            else if ((lastOffset + 1) < n && lastAllocatedPage != -1) {
                //there is enough space
                lastOffset++; //we only increment it
                pos = lastOffset;
                fpage = lastAllocatedPage;
            } else {
                /*otherwise, we have to alloc another page*/
                pid = -1;
                f_alloc_page_c(1, &pid);

                /*check possible errors*/
                if (pid == -1) {
                    _DEBUG(ERROR, "FlashDBSim: Failed to allocate page."
                            "There is no free page in the flash memory!");
                }
                lastAllocatedPage = pid;
                lastOffset = 0;

                pos = lastOffset;
                fpage = lastAllocatedPage;
            }
            //in addition, we store it in our mapping
            entry = (MapHighFlashPage*) lwalloc(sizeof (MapHighFlashPage));
            entry->flash_page = fpage;
            entry->flash_offset = pos;
            entry->idx_page = idx_page;
            HASH_ADD_INT(mhfp, idx_page, entry);
        }

        //note that we don't alloc new pages for the update or deletion case

        if (checker == -1) {
            //this is a remotion operation and thus, we possibly dealloc a flash page!
            //it only happens if there is a flash page with 0 nodes stored

            //in this case, we only change the values of these variables (the content of the removed node can be updated in the future)
            if (lastAllocatedPage == entry->flash_page && lastOffset == entry->flash_offset) {
                lastOffset--;
                if (lastOffset < 0) {
                    //we have to dealloc the lastAllocatedPage
                    f_release_page_c(entry->flash_page);
                    deadpage = true;
                }
            } else {
                FlashPagesWithSpace *aPage;
                HASH_FIND_INT(removedIndexPages, &(entry->flash_page), aPage);
                if (aPage == NULL) {
                    //this means that we store this flash_page in the hash table
                    aPage = (FlashPagesWithSpace*) lwalloc(sizeof (FlashPagesWithSpace));
                    aPage->flash_page = entry->flash_page;
                    aPage->n = 1;
                    aPage->positions = lwalloc(sizeof (int) * n);
                    HASH_ADD_INT(removedIndexPages, flash_page, aPage);
                } else {
                    //should we check the size of positions?
                    aPage->positions[n] = entry->flash_offset;
                    aPage->n++;
                }
            }

        }
        if (!deadpage) {
            /*since the granularity of a write is a page,
             we are not able to write only a part of this page
             thus, we have to read a page before its written*/
            uint8_t *page_content;
            if (si->gp->io_access == DIRECT_ACCESS) {
                //then the memory must be aligned in blocks!
                if (posix_memalign((void**) &page_content, simulator->page_size1, simulator->page_size1)) {
                    _DEBUG(ERROR, "Allocation failed at get_rnode");
                }
            } else {
                page_content = (uint8_t*) lwalloc(simulator->page_size1);
            }
            
            /*should we subtract statistical values from this read?
             probably not...*/
            rv = f_read_page_c(entry->flash_page, page_content, 0, simulator->page_size1);

            if (rv == RV_ERROR_INVALID_PAGE_STATE) {
                _DEBUG(ERROR, "FlashDBSim: page read is invalid");
            }
            if (rv == RV_ERROR_FLASH_BLOCK_BROKEN) {
                _DEBUG(ERROR, "FlashDBSim: the block contained the read page is broken");
            }

            /*we update its content*/
            memcpy(page_content + entry->flash_offset * si->gp->page_size, buf, si->gp->page_size);

            rv = f_write_page_c(entry->flash_page, page_content, 0, simulator->page_size1);
            if (rv == RV_ERROR_FLASH_NO_MEMORY) {
                _DEBUG(ERROR, "FlashDBSim: There is no space in the flash memory!");
            }

            if (si->gp->io_access == DIRECT_ACCESS) {
                free(page_content);
            } else {
                lwfree(page_content);
            }
        }

        if (checker == -1) {
            //in addition, we have to remove the entry of the mapping
            HASH_DEL(mhfp, entry);
            lwfree(entry);
        }

    }/* Third case: they are equal */
    else {
        /*in this case we have a equality situation between the page sizes
        Example:   flash simulator has a page size equal to 512bytes
        index employ a page size equal to 512bytes
        in this case, each page of the index corresponds to a flash page*/
        LBA pid; //the ids of the allocated flash pages
        MapLowFlashPage *entry;

        //_DEBUGF(NOTICE, "it is the case (iii) -> index page size %d and simulator page size %d",
        //        si->gp->page_size, simulator->page_size1);

        //we only alloc pages if this page is a newly created page in the flash
        HASH_FIND_INT(mlfp, &idx_page, entry);
        if (entry == NULL) {
            pid = -1;
            f_alloc_page_c(1, &pid);

            /*check possible errors*/
            if (pid == -1) {
                _DEBUG(ERROR, "FlashDBSim: Failed to allocate page."
                        "There is no free page in the flash memory!");
            }

            //in addition, we store it in our mapping
            entry = (MapLowFlashPage*) lwalloc(sizeof (MapLowFlashPage));
            entry->flash_page = pid;
            entry->idx_page = idx_page;
            HASH_ADD_INT(mlfp, idx_page, entry);
        }

        //note that we don't alloc new pages for the update or deletion case

        if (checker == -1) {
            //this is a remotion operation and thus, we have to dealloc the page!            
            f_release_page_c(entry->flash_page);
            //in addition, we have to remove the entries of the mapping
            HASH_DEL(mlfp, entry);
            lwfree(entry);
        } else {
            //this is an update or creation case: both cases we have to write in the flash pages            
            rv = f_write_page_c(entry->flash_page, buf, 0, simulator->page_size1);
            if (rv == RV_ERROR_FLASH_NO_MEMORY) {
                _DEBUG(ERROR, "FlashDBSim: There is no space in the flash memory!");
            }
        }
    }
    //_DEBUG(NOTICE, "flashdbsim write page done");
}

void flashdbsim_read_pages(const SpatialIndex *si, int *idx_pages, uint8_t *buf, int pagenum) {
    /*since the flashdbsim do not provide any function to sequentially read pages,
     we have to traverse each page and send individual requests*/
    int i;
    int page_size = si->gp->page_size;
    for (i = 0; i < pagenum; i++) {
        flashdbsim_read_one_page(si, idx_pages[i], buf + i * page_size);
    }
}

void flashdbsim_write_pages(const SpatialIndex *si, int *idx_pages, uint8_t *buf, int pagenum) {
    /*since the flashdbsim do not provide any function to sequentially write pages,
     we have to traverse each page and send individual requests*/
    int i;
    int page_size = si->gp->page_size;
    for (i = 0; i < pagenum; i++) {
        flashdbsim_write_one_page(si, buf + i * page_size, idx_pages[i]);
    }
}
