#/**********************************************************************
# *
# * FESTIval - Framework to Evaluate SpaTial Indices in non-VolAtiLe memories
# * https://accarniel.github.io/FESTIval/
# *
# * Copyright (C) 2016-2020 Anderson Chaves Carniel <accarniel@gmail.com>
# *
# * This is free software; you can redistribute and/or modify it under
# * the terms of the GNU General Public Licence. See the COPYING file.
# *
# * Fully developed by Anderson Chaves Carniel
# *
# **********************************************************************/

POSTGIS_SOURCE=$(postgis)
FLASHDBSIM_SOURCE=$(realpath libraries/Flash-DBSim-for-Linux-1.0)

$(shell python3 gen_config_h.py $(POSTGIS_SOURCE)/postgis_config.h)
$(info Compiling FlashDBSim...)
$(shell make -s -C $(FLASHDBSIM_SOURCE)/)
$(info Done.)

MODULE_big=festival-1.1
OBJS= \
    main/bbox_handler.o \
    main/storage_handler.o \
    main/io_handler.o \
    main/festival_util.o \
    main/header_handler.o \
    main/statistical_processing.o \
    rtree/rnode_stack.o \
    rtree/rnode.o \
    rtree/split.o \
    rtree/rtree.o \
    rstartree/rstartree.o \
    hilbertrtree/hilbert_curve.o \
    hilbertrtree/hilbert_node.o \
    hilbertrtree/hilbertnode_stack.o \
    hilbertrtree/hilbertrtree.o \
    hilbertrtree/hilbert_value.o \
    fast/fast_buffer.o \
    fast/fast_buffer_list_mod.o \
    fast/fast_flush_module.o \
    fast/fast_index.o \
    fast/fast_log_module.o \
    fast/fast_max_heap.o \
    fast/fast_redo_stack.o \
    fortree/fornode_stack.o \
    fortree/fortree.o \
    fortree/fortree_buffer.o \
    fortree/fortree_nodeset.o \
    libraries/rbtree-linux/rbtree.o \
    efind/efind_buffer_flushing_managers.o \
    efind/efind_index.o \
    efind/efind_log_manager.o \
    efind/efind_mod_handler.o \
    efind/efind_page_handler.o \
    efind/efind_page_handler_hilbertnode.o \
    efind/efind_page_handler_rnode.o \
    efind/efind_read_buffer_2q.o \
    efind/efind_read_buffer_hlru.o \
    efind/efind_read_buffer_lru.o \
    efind/efind_read_buffer_s2q.o \
    efind/efind_temporal_control.o \
    buffer/full2q.o \
    buffer/lru.o \
    buffer/hlru.o \
    buffer/s2q.o \
    postgres/query.o \
    postgres/execution.o \
    postgres/festival_module.o \
    flashdbsim/flashdbsim.o
EXTENSION = festival
DATA = festival--1.1.sql

# support versions of PostGIS lesser than 3.0 only (because of lwgeom dependency)
SHLIB_LINK = $(POSTGIS_SOURCE)/libpgcommon/libpgcommon.a $(POSTGIS_SOURCE)/postgis/postgis-*.so $(FLASHDBSIM_SOURCE)/C_API/libflashdb_capi.so -L/usr/local/lib -lgeos_c -lrt -llwgeom

PG_CPPFLAGS = -I/usr/local/include -I$(POSTGIS_SOURCE)/liblwgeom/ -I$(POSTGIS_SOURCE)/libpgcommon/ -I$(POSTGIS_SOURCE)/postgis/ -I$(FLASHDBSIM_SOURCE)/C_API/include/ -I/usr/include/ -fPIC

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
