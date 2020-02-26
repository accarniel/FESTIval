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
 * Fully developed by Anderson Chaves Carniel
 *
 **********************************************************************/

#include "../libraries/uthash/uthash.h" //for hashing functions
#include "fast_flush_module.h" //basic functions
#include "../main/log_messages.h" //for debug messages
#include "fast_buffer.h" //for buffer
#include "fast_log_module.h" //for log of FAST
#include "fast_index.h"

#include "../main/storage_handler.h" //for io handling
#include "../main/io_handler.h" //for DIRECT ACCESS
#include "fast_max_heap.h" //for max heap

#include "../main/statistical_processing.h"
#include <limits.h>
#include <float.h> //limit of INT_MAX

/*our all flushing units, which is an array of FLUSHING_UNIT*/
static FASTFlushingUnit *fast_flushing_units = NULL;
static int n_flushingunits = 0;
/* the max_heap as specified in the FAST paper -> it is only used for FAST_FF and FAST_STAR*/
static MaxHeap *flush_heap = NULL;

/* undefine the defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define to use the lwalloc and lwfree from the postgis */
#define uthash_malloc(sz) lwalloc(sz)
#define uthash_free(ptr,sz) lwfree(ptr)

#undef uthash_fatal
#define uthash_fatal(msg) _DEBUG(ERROR, msg)

/* this is an auxiliary struct in order to know:
 * what is the flushing unit of the rnode A?
 */
typedef struct FlushingUnitHandler {
    UT_hash_handle hh;

    int hash_key; //the rnode_page
    int hash_value; //the flushing unit identifier (index of the flushing_units array)    
} FlushingUnitHandler;

static FlushingUnitHandler *fuh = NULL;

static void init_flushing_units(int maxnodes);
static void alloc_flushing_units(int maxnodes);
static int get_nofmod_of_flushingunit(int fu);

void init_flushing_units(int maxnodes) {
    n_flushingunits = 1;
    fast_flushing_units = (FASTFlushingUnit*) lwalloc(sizeof (FASTFlushingUnit) * n_flushingunits);
    fast_flushing_units[0].n = 0;
    fast_flushing_units[0].node_pages = (int*) lwalloc(sizeof (int)*maxnodes);
}

void alloc_flushing_units(int maxnodes) {
    n_flushingunits++;
    fast_flushing_units = (FASTFlushingUnit*) lwrealloc(fast_flushing_units,
            sizeof (FASTFlushingUnit) * n_flushingunits);
    fast_flushing_units[n_flushingunits - 1].n = 0;
    fast_flushing_units[n_flushingunits - 1].node_pages = (int*) lwalloc(sizeof (int)*maxnodes);
}

/*this function gets the number of modifications of a flushing unit*/
int get_nofmod_of_flushingunit(int fu) {
    int i, mods = 0;
    for (i = 0; i < fast_flushing_units[fu].n; i++)
        mods += fb_get_nofmod(fast_flushing_units[fu].node_pages[i]);
    return mods;
}

void fast_set_flushing_unit(const FASTSpecification *spec, int node_page) {
    int i;
    FlushingUnitHandler *flu;
    if (n_flushingunits == 0) {
        init_flushing_units(spec->flushing_unit_size);
    }

    //we check if this node is contained in some flushing unit
    HASH_FIND_INT(fuh, &node_page, flu);
    if (flu == NULL) {
        //if this node is new... we put it in some flushing unit:
        //if we do not have space in the last flushing unit, then alloc one more flushing unit
        if (fast_flushing_units[n_flushingunits - 1].n >= spec->flushing_unit_size) {
            alloc_flushing_units(spec->flushing_unit_size);
        }
        i = n_flushingunits - 1;
        fast_flushing_units[i].n++;
        fast_flushing_units[i].node_pages[fast_flushing_units[i].n - 1] = node_page;

        flu = (FlushingUnitHandler*) lwalloc(sizeof (FlushingUnitHandler));
        flu->hash_key = node_page;
        flu->hash_value = i;
        HASH_ADD_INT(fuh, hash_key, flu); //we add it into the hash
    } else {
        /*this means that we have this node in flushing unit i*/
        i = flu->hash_value;
    }

    if (spec->flushing_policy == FAST_FLUSHING_POLICY) {
        //we add to or update a new flushing unit and to the heap
        int index;
        int hv = 0;
        if (flush_heap == NULL) {
            flush_heap = create_maxheap(n_flushingunits);
        }

        //we update (or insert when the flushing unit was created) the value of the heap for the flushing unit i        
        for (index = 0; index < fast_flushing_units[i].n; index++)
            hv += fb_get_nofmod(fast_flushing_units[i].node_pages[index]);
        modify_maxheap(flush_heap, i + 1, hv);
    } else if (spec->flushing_policy == FAST_STAR_FLUSHING_POLICY) {
        //we add to a new flushing unit and consider the timestamp
        int index;
        int nu = 0;
        int ts = 0.0;
        int hv;
        struct timespec tim;
        if (flush_heap == NULL)
            flush_heap = create_maxheap(n_flushingunits);

        //we update (or insert when the flushing unit was created) the value of the heap for the flushing unit i        
        //nu = the number of updates of a flushing unit
        for (index = 0; index < fast_flushing_units[i].n; index++)
            nu += fb_get_nofmod(fast_flushing_units[i].node_pages[index]);
        //lastimestamp = the most recent update of a flushing unit in ms (s, ns and us were worst)
        clock_gettime(CLOCK_MONOTONIC, &tim);
        ts = (int) 1e3 * tim.tv_sec + tim.tv_nsec * 1e-6;

        if (nu <= 0) {
            //this indicates that a node has no modifications and thus, cannot be chosen
            hv = -INT_MAX;
        } else {
            //we maximize the number of updates and minimize the timestamp:
            hv = nu - ts;
        }
        //we insert/update the hv value in the maxheap
        modify_maxheap(flush_heap, i + 1, hv);
    } else if (spec->flushing_policy != FLUSH_ALL && spec->flushing_policy != RANDOM_FLUSH) {
        _DEBUGF(ERROR, "There is no this flushing_policy: %d", spec->flushing_policy);
    }
}
/* return a random number between 0 and limit inclusive.
 */
static int rand_lim(int limit);

int rand_lim(int limit) {
    int divisor = RAND_MAX / (limit + 1);
    int retval;

    /* initialize random seed: */
    srand(time(NULL));

    do {
        retval = rand() / divisor;
    } while (retval > limit);

    return retval;
}

void fast_execute_flushing(const SpatialIndex *base, FASTSpecification *spec) {
    int i, n = 0;
    int chosen_fu = 0;
    int mods;
    uint8_t *buf, *loc;
    size_t buf_size;
    uint8_t index_type;
#ifdef COLLECT_STATISTICAL_DATA
    struct timespec cpustart;
    struct timespec cpuend;
    struct timespec start;
    struct timespec end;

    cpustart = get_CPU_time();
    start = get_current_time();

    _flushing_num++;
#endif

    index_type = spatialindex_get_type(base);
    
    if (spec->flushing_policy == RANDOM_FLUSH) {
        bool valid_fu = false;
        /* we simply get any flushing unit 
         * that have some modification in the buffer 
         * to be written in the disk*/
        while (!valid_fu) {
            chosen_fu = rand_lim(n_flushingunits - 1);

            if (chosen_fu >= n_flushingunits) {
                _DEBUGF(ERROR, "The chosen flushing unit %d is higher than the number of flushing units %d",
                        chosen_fu, n_flushingunits);
                return;
            }

            mods = get_nofmod_of_flushingunit(chosen_fu);
            if (mods > 0)
                valid_fu = true;
            else
                valid_fu = false;
        }
        n = fast_flushing_units[chosen_fu].n;
        buf_size = n * base->gp->page_size;
    } else if (spec->flushing_policy == FAST_FLUSHING_POLICY ||
            spec->flushing_policy == FAST_STAR_FLUSHING_POLICY) {
        //we add to a new flushing unit and to the heap
        chosen_fu = get_maxheap(flush_heap).fu - 1;

        if (chosen_fu >= n_flushingunits) {
            _DEBUGF(ERROR, "The chosen flushing unit %d is higher than the number of flushing units %d",
                    chosen_fu, n_flushingunits);
            return;
        }

        n = fast_flushing_units[chosen_fu].n;
        buf_size = n * base->gp->page_size;
    } else if (spec->flushing_policy == FLUSH_ALL) {
        buf_size = 0;
        for (i = 0; i < n_flushingunits; i++) {
            //we will only flush out the flushing units with modifications
            mods = get_nofmod_of_flushingunit(i);
            /*limitation: if a flushing unit has a node without modification!*/
            if (mods > 0) {
                n = fast_flushing_units[i].n;
                buf_size += (n * base->gp->page_size);
            }
        }
    } else {
        _DEBUGF(ERROR, "There is no this flushing_policy: %d", spec->flushing_policy);
        return;
    }

    if (base->gp->io_access == DIRECT_ACCESS) {
        //then the memory must be aligned in blocks!
        if (posix_memalign((void**) &buf, base->gp->page_size, buf_size)) {
            _DEBUG(ERROR, "Allocation failed at execute_flushing");
            return;
        }
    } else {
        buf = (uint8_t*) lwalloc(buf_size);
    }

    //we write sequentially here.......
    if (spec->flushing_policy == FLUSH_ALL) {
        int j;
        int npages;
        int count;
        int *pages = NULL;
        int *heights = NULL;
        void *node = NULL;

        loc = buf;

        pages = (int*) lwalloc(sizeof (int));
        heights = (int*) lwalloc(sizeof (int));

        npages = 0;
        count = 0;
        for (j = 0; j < n_flushingunits; j++) {
            mods = get_nofmod_of_flushingunit(j);
            if (mods > 0) {
                n = fast_flushing_units[j].n;
                npages += n;
                pages = (int*) lwrealloc(pages, sizeof (int)*npages);
                heights = (int*) lwrealloc(heights, sizeof (int)*npages);
                for (i = 0; i < n; i++) {
                    pages[count] = fast_flushing_units[j].node_pages[i];
                    heights[count] = fb_get_node_height(pages[count]);
                    node = fb_retrieve_node(base, pages[count], heights[count]);

                    if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                        rnode_serialize((RNode *) node, loc);
                        if (node != NULL) {
                            rnode_free((RNode *) node);
                        }
                    } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                        hilbertnode_serialize((HilbertRNode *) node, loc);
                        if (node != NULL) {
                            hilbertnode_free((HilbertRNode *) node);
                        }
                    }
                    loc += base->gp->page_size;
                    count++;
                }
            }
        }

        storage_write_pages(base, pages, buf, heights, npages);

        //we also write about this flushing operation in the log file
        write_log_flush(base, spec, pages, npages);

#ifdef COLLECT_STATISTICAL_DATA
        _flushed_nodes_num += npages;
#endif

        //we remove them from the buffer
        for (i = 0; i < npages; i++) {
            fb_free_hashvalue(pages[i], index_type);
        }

        //we set the flushing units of each page (these values will be 0)
        for (i = 0; i < npages; i++) {
            fast_set_flushing_unit(spec, pages[i]);
        }

        lwfree(pages);
        lwfree(heights);
    } else {
        void *node = NULL;
        int *heights = NULL;
        
        //_DEBUG(NOTICE, "executing the flushing operation....");

        heights = (int*) lwalloc(sizeof (int) * n);

        loc = buf;

        for (i = 0; i < n; i++) {
            //_DEBUGF(NOTICE, "GETTING THE NODE %d to be flushed", fast_flushing_units[chosen_fu].node_pages[i]);            

            heights[i] = fb_get_node_height(fast_flushing_units[chosen_fu].node_pages[i]);
            node = fb_retrieve_node(base, fast_flushing_units[chosen_fu].node_pages[i], heights[i]);

            if (index_type == FAST_RTREE_TYPE || index_type == FAST_RSTARTREE_TYPE) {
                //rnode_print(node, fast_flushing_units[chosen_fu].node_pages[i]);
                rnode_serialize((RNode *) node, loc);
                if (node != NULL)
                    rnode_free((RNode *) node);
            } else if (index_type == FAST_HILBERT_RTREE_TYPE) {
                hilbertnode_serialize((HilbertRNode *) node, loc);
                if (node != NULL) {
                    hilbertnode_free((HilbertRNode *) node);
                }
            }
            loc += base->gp->page_size;
        }
       
        storage_write_pages(base, fast_flushing_units[chosen_fu].node_pages, buf, heights, n);
        //disk_write(&fs, fast_flushing_units[chosen_fu].node_pages, buf, n);

        //we also write about this flushing operation in the log file
        write_log_flush(base, spec, fast_flushing_units[chosen_fu].node_pages, n);
        
#ifdef COLLECT_STATISTICAL_DATA
        _flushed_nodes_num += n;
#endif
        lwfree(heights);

        //we remove them from the buffer
        for (i = 0; i < n; i++) {
            fb_free_hashvalue(fast_flushing_units[chosen_fu].node_pages[i], index_type);
        }

        //we update its flushing unit (it will has the value 0)
        fast_set_flushing_unit(spec, fast_flushing_units[chosen_fu].node_pages[0]);
    }

    if (base->gp->io_access == DIRECT_ACCESS) {
        free(buf);
    } else {
        lwfree(buf);
    }

#ifdef COLLECT_STATISTICAL_DATA
    cpuend = get_CPU_time();
    end = get_current_time();

    _flushing_cpu_time += get_elapsed_time(cpustart, cpuend);
    _flushing_time += get_elapsed_time(start, end);
#endif
}

void fast_flush_all(const SpatialIndex *base, FASTSpecification *spec) {
    //in this case we simply call the function
    if (spec->flushing_policy == FLUSH_ALL) {
        fast_execute_flushing(base, spec);
    } else {
        uint8_t tmp;
        //otherwise, we temporally modify the policy for the flush_all
        tmp = spec->flushing_policy;
        spec->flushing_policy = FLUSH_ALL;
        fast_execute_flushing(base, spec);
        spec->flushing_policy = tmp;
    }
}

void fast_destroy_flushing() {
    FlushingUnitHandler *entry, *temp;
    int i;
    if (fast_flushing_units != NULL) {
        for (i = 0; i < n_flushingunits; i++)
            lwfree(fast_flushing_units[i].node_pages);

        lwfree(fast_flushing_units);
    }

    if (flush_heap != NULL) {
        destroy_maxheap(flush_heap);
    }

    HASH_ITER(hh, fuh, entry, temp) {
        HASH_DEL(fuh, entry);
        //a well-known bug: if we try to free this entry, we have undefined behavior in some time
        //therefore, we prefer some memory leak than fatal error
        //it only happens for fast r* tree indices
        //TODO Check it later with valgrind
        lwfree(entry);
    }

    fast_flushing_units = NULL;
    n_flushingunits = 0;
    flush_heap = NULL;
}
