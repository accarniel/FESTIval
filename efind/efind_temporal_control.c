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

/*
 * This file implements the temporal control employed by eFIND
 */

#include "efind_temporal_control.h" //for basic operations
#include "efind_flushing_manager.h" //for the chosenPage structure
#include "efind_buffer_manager.h" //for operation of the read buffer

#include "../libraries/uthash/uthash.h" //for temporal control for reads

#include "../main/log_messages.h" //for messages
#include "../main/statistical_processing.h" //for collection of statistical data

/* we have two lists for the temporal control -> reads and writes
 they are FIFO lists
 * NOTE 1 - since for reads we have some additional operations, 
 * this list is implemented as a UTHASH (which is based on the doubled linked list)
 * NOTE 2 - since for writes we have only add and iterate operations, 
 * this list is implemented as a common linked list
 * NOTE 3 - the size of the temporal control for reads is variable, with a minimum of 10 elements.
 * This means that it can change in size (spec->read_temporal_control_size) in some places.
 */

/* basic structure for TEMPORAL CONTROL FOR READS */
typedef struct {
    UT_hash_handle hh;
    int page_id; //the key
} eFINDReadTemporalControl;

/* basic structure for TEMPORAL CONTROL FOR WRITES */
typedef struct _efind_tc_item {
    int page_id;
    struct _efind_tc_item *next;
} eFINDTemporalItem;

typedef struct {
    int total_size;
    int current_size;
    eFINDTemporalItem *head;
    eFINDTemporalItem *tail;
} eFINDWriteTemporalControl;

/* variables for the temporal control management */
static eFINDReadTemporalControl *read_temporal_control = NULL;
static eFINDWriteTemporalControl *write_temporal_control = NULL;

static unsigned int read_temporal_control_size = MINIMUM_READ_TEMPORAL_CONTROL_SIZE;

void efind_add_read_temporal_control(const eFINDSpecification *spec, int p_id) {
    if (spec->temporal_control_policy == eFIND_READ_TCP ||
            spec->temporal_control_policy == eFIND_READ_WRITE_TCP) {
        eFINDReadTemporalControl *entry;
        int new_size = 0;

        HASH_FIND_INT(read_temporal_control, &p_id, entry);

        if (entry != NULL) {
            //we do nothing since it is FIFO
        } else {
            //we check if we have enough space
            if (HASH_COUNT(read_temporal_control) >= read_temporal_control_size) {
                eFINDReadTemporalControl *tmp_entry;
                //we don't have, we therefore remove from the head

                HASH_ITER(hh, read_temporal_control, entry, tmp_entry) {
                    // delete entries until we have space
                    if (HASH_COUNT(read_temporal_control) >= read_temporal_control_size) {
                        HASH_DEL(read_temporal_control, entry);
                        lwfree(entry);
                    } else {
                        break;
                    }
                }
            }
            entry = (eFINDReadTemporalControl*) lwalloc(sizeof (eFINDReadTemporalControl));
            entry->page_id = p_id;
            HASH_ADD_INT(read_temporal_control, page_id, entry);
        }
        //we then update the size of read_temporal_control_size, if needed
        new_size = ceil((efind_readbuffer_number_of_elements(spec) +
                efind_writebuffer_number_of_elements()) * (spec->read_temporal_control_perc / 100.0));
        if (new_size > MINIMUM_READ_TEMPORAL_CONTROL_SIZE) {
            read_temporal_control_size = new_size;
        }
    }
}

static void efind_destroy_read_temporal_control() {
    eFINDReadTemporalControl *tmp_entry, *entry;

    HASH_ITER(hh, read_temporal_control, entry, tmp_entry) {
        HASH_DEL(read_temporal_control, entry);
        lwfree(entry);
    }
}

uint8_t efind_read_temporal_control_contains(const eFINDSpecification *spec, int page_id) {
    uint8_t ret = NOT_INSERTED;
    if (spec->temporal_control_policy == eFIND_READ_TCP ||
            spec->temporal_control_policy == eFIND_READ_WRITE_TCP) {
        /*in this case, we will force the storage of the "node' in the read buffer if:
         the node written into the storage device is contained in the read_temporal_control*/
        eFINDReadTemporalControl *entry;
        HASH_FIND_INT(read_temporal_control, &page_id, entry);
        //we have to update this value
        if (entry != NULL) {
            ret = INSERTED;
        }
    } else {
        //we show an error since for S2Q with eFIND we should handle temporal control for reads
        _DEBUG(ERROR, "eFIND has not temporal control for reads enabled.");
    }
    return ret;
}

void efind_read_temporal_control_remove(const eFINDSpecification *spec, int page_id) {
    if (spec->temporal_control_policy == eFIND_READ_TCP ||
            spec->temporal_control_policy == eFIND_READ_WRITE_TCP) {
        /*in this case, we will force the storage of the "node' in the read buffer if:
         the node written into the storage device is contained in the read_temporal_control*/
        eFINDReadTemporalControl *entry;
        HASH_FIND_INT(read_temporal_control, &page_id, entry);
        //we have to update this value
        if (entry != NULL) {
            HASH_DEL(read_temporal_control, entry);
            lwfree(entry);
        }
    } else {
        //we show an error since for S2Q with eFIND we should handle temporal control for reads
        _DEBUG(ERROR, "eFIND has not temporal control for reads enabled.");
    }
}

void efind_add_write_temporal_control(const eFINDSpecification *spec, int node_id) {
    if (spec->temporal_control_policy == eFIND_WRITE_TCP ||
            spec->temporal_control_policy == eFIND_READ_WRITE_TCP) {
        eFINDTemporalItem *ti = lwalloc(sizeof (eFINDTemporalItem));

        if (write_temporal_control == NULL) {
            write_temporal_control = (eFINDWriteTemporalControl*) lwalloc(sizeof (eFINDWriteTemporalControl));
            write_temporal_control->head = NULL;
            write_temporal_control->tail = NULL;
            write_temporal_control->total_size = spec->write_temporal_control_size;
            write_temporal_control->current_size = 0;
        }

        ti->page_id = node_id;
        ti->next = NULL;

        /*in this case, this list is empty*/
        if (write_temporal_control->head == NULL) {
            write_temporal_control->head = ti;
            write_temporal_control->tail = ti;
            write_temporal_control->current_size++;
        } else {
            //we insert at the end of this list
            write_temporal_control->tail->next = ti;
            write_temporal_control->tail = ti;

            write_temporal_control->current_size++;

            //we have to check if we have enough space
            if (write_temporal_control->current_size > write_temporal_control->total_size) {
                eFINDTemporalItem *temp;
                temp = write_temporal_control->head;
                write_temporal_control->head = temp->next;
                lwfree(temp);

                write_temporal_control->current_size = write_temporal_control->total_size;
            }
        }
    }
}

/*destroy a temporal control list*/
static void efind_destroy_write_temporal_control() {
    if (write_temporal_control != NULL) {
        eFINDTemporalItem *current;
        while (write_temporal_control->head != NULL) {
            current = write_temporal_control->head;
            write_temporal_control->head = current->next;
            lwfree(current);
        }
        lwfree(write_temporal_control);
        write_temporal_control = NULL;
    }
}

/*this function checks if we should store this node in the read buffer in order to avoid future reads after writes*/
uint8_t efind_temporal_control_for_reads(const SpatialIndex *base, const eFINDSpecification *spec,
        int page_id, int height, void *page, uint8_t index_type) {
    uint8_t ret = NOT_INSERTED;
    if (read_temporal_control != NULL && (spec->temporal_control_policy == eFIND_READ_TCP ||
            spec->temporal_control_policy == eFIND_READ_WRITE_TCP) && page != NULL) {
        /*in this case, we will force the storage of the "node' in the read buffer if:
         the node written into the storage device is contained in the read_temporal_control*/
        eFINDReadTemporalControl *entry;
        HASH_FIND_INT(read_temporal_control, &page_id, entry);
        //we have to update this value
        if (entry != NULL) {
            UIPage *p = efind_pagehandler_create(page, index_type);
            efind_put_node_in_readbuffer(base, spec, p, page_id, height, true);
            lwfree(p);
            ret = INSERTED;
#ifdef COLLECT_STATISTICAL_DATA
            _efind_force_node_in_read_buffer++;
#endif
        }

    }
    return ret;
}

ChosenPage *efind_temporal_control_for_writes(const eFINDSpecification *spec,
        const ChosenPage *raw, int n, int *n_ret) {
    if (spec->temporal_control_policy == eFIND_WRITE_TCP ||
            spec->temporal_control_policy == eFIND_READ_WRITE_TCP) {
        /*the temporal control only occurs if we have temporal control for writes */
        if (write_temporal_control == NULL ||
                write_temporal_control->current_size == 0) {
            //in this case, we don't have collected any writes yet, then we return a copy of raw here
            ChosenPage *ret;
            ret = (ChosenPage*) lwalloc(n * sizeof (ChosenPage));
            memcpy(ret, raw, n * sizeof (ChosenPage));
            *n_ret = n;
            return ret;
        } else {
            //otherwise, we do the treatment for the following 2 cases
            int i;
            eFINDTemporalItem *current;
            bool is_seq = false;
            bool is_stride = false;

            //we create two arrays for each case
            ChosenPage *seq, *stride;
            int is = 0, ist = 0;
            seq = (ChosenPage*) lwalloc(n * sizeof (ChosenPage));
            stride = (ChosenPage*) lwalloc(n * sizeof (ChosenPage));

            //for each page in our raw list
            for (i = 0; i < n; i++) {
                is_seq = false;
                is_stride = false;
                //for each page in our temporal control for writes
                current = write_temporal_control->head;
                while (current != NULL) {
                    //we only consider pages that were not written before
                    if (current->page_id != raw[i].page_id) {
                        /*
                         * We now determine where we will put the page from the raw list...
                         * there are two options, where the first option is stronger than the second
                         * 
                         * 1 - we put into sequence list the pages that provides a sequential or partial (semi-)sequential write (according to a distance)
                         * 
                         * according to:
                         * Bouganim, Luc, Björn Jónsson, and Philippe Bonnet. "uFLIP: Understanding flash IO patterns." arXiv preprint arXiv:0909.1780 (2009).
                         * 
                         * and
                         * 
                         * Dubs, Paul, et al. "FBARC: I/O Asymmetry Aware Buffer Replacement Strategy." ADMS@ VLDB. 2013.
                         * 
                         */
                        if (abs(current->page_id - raw[i].page_id) <= spec->write_tc_minimum_distance) {
                            is_seq = true;
                        } else
                            /*
                             * 2 - we put into the stride list all the pages that provides a stride write according to a distance
                             * stride write is not so fast as sequencial write, but it is faster than other types of writes, according to
                             * 
                             * Chen, F., Koufaty, D.A., Zhang, X.: Understanding intrinsic characteristics and
system implications of flash memory based solid state drives. In: Int. Conf. on
Measurement and Modeling of Computer Systems. pp. 181–192 (2009)
                             * 
                             * and
                             * 
                             * Jung, M., Kandemir, M.: Revisiting widely held SSD expectations and rethinking
system-level implications. In: Int. Conf. on Measurement and Modeling of Com-
puter Systems. pp. 203–216 (2013)
                             * 
                             */

                            if (abs(current->page_id - raw[i].page_id) >= spec->write_tc_stride) {
                            is_stride = true;
                        }
                    }
                    current = current->next;
                }
                //a page cannot belongs to the both lists
                if (is_seq) {
                    seq[is] = raw[i];
                    is++;
                } else if (is_stride) {
                    stride[ist] = raw[i];
                    ist++;
                }
            }
            //if the case 1 provides enough pages to create at least 1 flushing unit, we return it
            if (is > ist && is >= spec->flushing_unit_size) {
#ifdef COLLECT_STATISTICAL_DATA
                _efind_write_temporal_control_sequential++;
#endif
                *n_ret = is;

                lwfree(stride);

                return seq;
            } else
                //else if, the case 2 provides enough pages to create at least 1 flushing unit, we return it
                if (ist >= spec->flushing_unit_size) {
#ifdef COLLECT_STATISTICAL_DATA
                _efind_write_temporal_control_stride++;
#endif
                *n_ret = ist;

                lwfree(seq);

                return stride;
            } else
                //else if, the union between the lists provide the creation of 1 flushing unit, we return this union
                if (is + ist >= spec->flushing_unit_size) {
                ChosenPage *ret;

#ifdef COLLECT_STATISTICAL_DATA
                _efind_write_temporal_control_seqstride++;
#endif

                ret = (ChosenPage*) lwalloc((is + ist) * sizeof (ChosenPage));
                memcpy(ret, seq, is * sizeof (ChosenPage));
                memcpy(ret + is, stride, ist * sizeof (ChosenPage));
                *n_ret = is + ist;

                lwfree(seq);
                lwfree(stride);

                return ret;
            } else {
                //otherwise, a temporal control is not able to 
                //create a flushing unit according to the writes, 
                //we just return a copy of the raw pages
                ChosenPage *ret;

#ifdef COLLECT_STATISTICAL_DATA
                _efind_write_temporal_control_filled++;
#endif

                ret = (ChosenPage*) lwalloc(n * sizeof (ChosenPage));
                memcpy(ret, raw, n * sizeof (ChosenPage));
                *n_ret = n;

                lwfree(seq);
                lwfree(stride);

                return ret;
            }
        }
    } else {
        //in this case, we don't have temporal control support, then just return NULL
        return NULL;
    }
}

void efind_temporal_control_destroy() {
    efind_destroy_read_temporal_control();
    efind_destroy_write_temporal_control();
}
