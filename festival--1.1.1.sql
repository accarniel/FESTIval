--/**********************************************************************
-- *
-- * FESTIval - Framework to Evaluate SpaTial Indices in non-VolAtiLe memories and hard disk drives.
-- * https://accarniel.github.io/FESTIval/
-- *
-- * Copyright (C) 2016-2021 Anderson Chaves Carniel <accarniel@gmail.com>
-- *
-- * This is free software; you can redistribute and/or modify it under
-- * the terms of the GNU General Public Licence. See the COPYING file.
-- *
-- * Fully developed by Anderson Chaves Carniel
-- *
-- **********************************************************************/

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION festival" to load this file. \quit

--------------------------------------------------------------------
-- CREATION OF ALL CONFIGURATIONS AND THE STATISTICAL DATA TABLES --
--------------------------------------------------------------------

--the FESTIval data schema stores configurations of different spatial indices as well as statistical data related to the execution of performed operations
CREATE SCHEMA fds;

CREATE TABLE fds.Source (
  src_id INTEGER NOT NULL,
  schema_name VARCHAR NOT NULL,
  table_name VARCHAR NOT NULL,
  column_name VARCHAR NOT NULL,
  pk_name VARCHAR NOT NULL,
  PRIMARY KEY(src_id)
);

CREATE TABLE fds.SpecializedConfiguration (
  sc_id INTEGER NOT NULL,
  description VARCHAR NULL,
  PRIMARY KEY(sc_id)
);

CREATE TABLE fds.StorageSystem (
  ss_id INTEGER NOT NULL,
  storage_system VARCHAR NOT NULL,
  description VARCHAR NULL,
  PRIMARY KEY(ss_id)
);

CREATE TABLE fds.BasicConfiguration (
  bc_id INTEGER NOT NULL,
  ss_id INTEGER NOT NULL,
  page_size INTEGER NOT NULL,
  io_access VARCHAR NOT NULL CHECK (upper(io_access) IN ('DIRECT ACCESS', 'NORMAL ACCESS')),
  refinement_type VARCHAR NOT NULL CHECK (upper(refinement_type) IN ('ONLY GEOS', 'GEOS AND POSTGIS')),
  PRIMARY KEY(bc_id),
  FOREIGN KEY(ss_id)
    REFERENCES fds.StorageSystem(ss_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.VirtualFlashDevice (
  vfd_id INTEGER NOT NULL,
  nand_device_type INTEGER NOT NULL CHECK (nand_device_type BETWEEN 1 AND 4),
  block_count INTEGER NOT NULL CHECK (block_count > 0),
  page_count_per_block INTEGER NOT NULL CHECK (page_count_per_block > 0),
  page_size1 INTEGER NOT NULL CHECK (page_size1 > 0),
  page_size2 INTEGER NOT NULL,
  erase_limitation INTEGER NOT NULL CHECK(erase_limitation > 0),
  read_random_time INTEGER NOT NULL CHECK (read_random_time >= 0),
  read_serial_time INTEGER NOT NULL CHECK (read_serial_time >= 0),
  program_time INTEGER NOT NULL CHECK (program_time >= 0),
  erase_time INTEGER NOT NULL CHECK (erase_time >= 0),
  PRIMARY KEY(vfd_id)
);

CREATE TABLE fds.FlashTranslationLayer (
  ftl_id INTEGER NOT NULL,
  ftl_type INTEGER NOT NULL CHECK (ftl_type = 1),
  map_list_size INTEGER NOT NULL CHECK (map_list_size > 0),
  wear_leveling_threshold INTEGER NOT NULL CHECK (wear_leveling_threshold > 0),
  PRIMARY KEY(ftl_id)
);

--TODO Create a TRIGGER to validate the combination of FTL and VFD (e.g., map_list_size should be equal to block_count * page_count_per_block for the ftl_type = 1)
CREATE TABLE fds.FlashDBSimConfiguration (
  ss_id INTEGER NOT NULL,
  ftl_id INTEGER NOT NULL,
  vfd_id INTEGER NOT NULL,
  PRIMARY KEY(ss_id, ftl_id, vfd_id),
  FOREIGN KEY(ss_id)
    REFERENCES fds.StorageSystem(ss_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION,
  FOREIGN KEY(vfd_id)
    REFERENCES fds.VirtualFlashDevice(vfd_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION,
  FOREIGN KEY(ftl_id)
    REFERENCES fds.FlashTranslationLayer(ftl_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.BufferConfiguration (
  buf_id INTEGER NOT NULL,
  buf_type VARCHAR NOT NULL CHECK (upper(buf_type) IN ('NONE', 'LRU', 'HLRU') OR upper(buf_type) LIKE 'S2Q%' OR upper(buf_type) LIKE '2Q%'),
  buf_size INTEGER NOT NULL CHECK (buf_size >= 0),
  PRIMARY KEY(buf_id)
);

--TODO Create a TRIGGER to validate the combination of index_type and db_sc_id
CREATE TABLE fds.FastConfiguration (
  sc_id INTEGER NOT NULL,
  index_type VARCHAR NOT NULL,
  db_sc_id INTEGER NOT NULL,
  buffer_size INTEGER NOT NULL CHECK (buffer_size > 0),
  flushing_unit_size INTEGER NOT NULL CHECK (flushing_unit_size > 0),
  flushing_policy VARCHAR NOT NULL CHECK (flushing_policy IN ('FLUSH ALL', 'RANDOM FLUSH', 'FAST FLUSHING POLICY', 'FAST STAR FLUSHING POLICY')),
  log_size INTEGER NOT NULL CHECK (log_size >= 0),
  PRIMARY KEY(sc_id),
  FOREIGN KEY(sc_id)
    REFERENCES fds.SpecializedConfiguration(sc_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

--TODO Create a TRIGGER to validate the combination of index_type and db_sc_id
CREATE TABLE fds.eFINDConfiguration (
  sc_id INTEGER NOT NULL,
  index_type VARCHAR NOT NULL,
  db_sc_id INTEGER NOT NULL,
  buffer_size INTEGER NOT NULL CHECK (buffer_size > 0),
  read_buffer_perc DOUBLE PRECISION NOT NULL CHECK(read_buffer_perc >= 0),
  temporal_control_policy VARCHAR NOT NULL CHECK (upper(temporal_control_policy) IN ('TEMPORAL CONTROL NONE', 'TEMPORAL CONTROL FOR READS', 'TEMPORAL CONTROL FOR WRITES', 'TEMPORAL CONTROL FOR READS AND WRITES')),
  read_temporal_control_perc DOUBLE PRECISION NOT NULL,
  write_temporal_control_size INTEGER NOT NULL,
  write_temporal_control_mindist INTEGER NOT NULL,
  write_temporal_control_stride INTEGER NOT NULL,
  timestamp_percentage DOUBLE PRECISION NOT NULL CHECK (timestamp_percentage BETWEEN 0 AND 100),
  flushing_unit_size INTEGER NOT NULL CHECK (flushing_unit_size > 0),
  flushing_policy VARCHAR NOT NULL CHECK (upper(flushing_policy) IN ('EFIND FLUSH MOD', 'EFIND FLUSH MOD TIME', 'EFIND FLUSH MOD TIME HEIGHT', 'EFIND FLUSH MOD TIME HEIGHT AREA', 'EFIND FLUSH MOD TIME HEIGHT AREA OVERLAP')),
  read_buffer_policy VARCHAR NULL CHECK (read_buffer_policy is NULL or upper(read_buffer_policy) IN ('NONE', 'EFIND LRU', 'EFIND HLRU', 'EFIND S2Q') or upper(read_buffer_policy) LIKE 'EFIND 2Q%'),
  log_size INTEGER NOT NULL CHECK(log_size >= 0),
  PRIMARY KEY(sc_id),
  FOREIGN KEY(sc_id)
    REFERENCES fds.SpecializedConfiguration(sc_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.OccupancyRate (
  or_id INTEGER NOT NULL,
  min_fill_int_nodes DOUBLE PRECISION NOT NULL CHECK (min_fill_int_nodes BETWEEN 0 AND 100),
  min_fill_leaf_nodes DOUBLE PRECISION NOT NULL CHECK (min_fill_leaf_nodes BETWEEN 0 AND 100),
  max_fill_int_nodes DOUBLE PRECISION NOT NULL CHECK (max_fill_int_nodes BETWEEN 5 AND 100),
  max_fill_leaf_nodes DOUBLE PRECISION NOT NULL CHECK (max_fill_leaf_nodes BETWEEN 5 AND 100),
  PRIMARY KEY(or_id)
);

CREATE TABLE fds.ForTreeConfiguration (
  sc_id INTEGER NOT NULL,
  or_id INTEGER NOT NULL,
  buffer_size INTEGER NOT NULL CHECK (buffer_size > 0),
  flushing_unit_size INTEGER NOT NULL CHECK (flushing_unit_size > 0),
  ratio_flushing DOUBLE PRECISION NOT NULL CHECK (ratio_flushing BETWEEN 0 AND 100),
  x DOUBLE PRECISION NOT NULL CHECK (x > 0),
  y DOUBLE PRECISION NOT NULL CHECK (y > 0),
  PRIMARY KEY(sc_id),
  FOREIGN KEY(sc_id)
    REFERENCES fds.SpecializedConfiguration(sc_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION,
  FOREIGN KEY(or_id)
    REFERENCES fds.OccupancyRate(or_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

--TODO improve the check for the reinsertion (i.e., take into account the maximum number of entries of a node and the minimum fill of a node)
CREATE TABLE fds.RStarTreeConfiguration (
  sc_id INTEGER NOT NULL,
  or_id INTEGER NOT NULL,
  reinsertion_perc_internal_node DOUBLE PRECISION NOT NULL CHECK (reinsertion_perc_internal_node >= 0),
  reinsertion_perc_leaf_node DOUBLE PRECISION NOT NULL CHECK (reinsertion_perc_leaf_node >= 0),
  reinsertion_type VARCHAR NOT NULL CHECK (upper(reinsertion_type) IN ('FAR REINSERT', 'CLOSE REINSERT')),
  max_neighbors_exam INTEGER NOT NULL,
  PRIMARY KEY(sc_id),
  FOREIGN KEY(sc_id)
    REFERENCES fds.SpecializedConfiguration(sc_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION,
  FOREIGN KEY(or_id)
    REFERENCES fds.OccupancyRate(or_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.HilbertRTreeConfiguration (
  sc_id INTEGER NOT NULL,
  or_id INTEGER NOT NULL,
  order_splitting_policy INTEGER NOT NULL CHECK (order_splitting_policy>=1),
  PRIMARY KEY(sc_id),
  FOREIGN KEY(sc_id)
    REFERENCES fds.SpecializedConfiguration(sc_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION,
  FOREIGN KEY(or_id)
    REFERENCES fds.OccupancyRate(or_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.RTreeConfiguration (
  sc_id INTEGER NOT NULL,
  or_id INTEGER NOT NULL,
  split_type VARCHAR NOT NULL CHECK (upper(split_type) IN ('EXPONENTIAL', 'LINEAR', 'QUADRATIC', 'RSTARTREE SPLIT', 'GREENE SPLIT', 'ANGTAN SPLIT')),
  PRIMARY KEY(sc_id),
  FOREIGN KEY(sc_id)
    REFERENCES fds.SpecializedConfiguration(sc_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION,
  FOREIGN KEY(or_id)
    REFERENCES fds.OccupancyRate(or_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.IndexConfiguration (
  config_id SERIAL,
  src_id INTEGER NOT NULL,
  sc_id INTEGER NOT NULL,
  bc_id INTEGER NOT NULL,
  buf_id INTEGER NOT NULL,
  PRIMARY KEY(config_id),
  FOREIGN KEY(bc_id)
    REFERENCES fds.BasicConfiguration(bc_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION,
  FOREIGN KEY(sc_id)
    REFERENCES fds.SpecializedConfiguration(sc_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION,
  FOREIGN KEY(src_id)
    REFERENCES fds.Source(src_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION,
  FOREIGN KEY(buf_id)
    REFERENCES fds.BufferConfiguration(buf_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.SpatialIndex (
  idx_id SERIAL,
  config_id INTEGER NOT NULL,
  idx_name VARCHAR NOT NULL,
  idx_path VARCHAR NOT NULL,
  idx_creation TIMESTAMP NOT NULL,
  idx_last_mod TIMESTAMP NOT NULL,
  PRIMARY KEY(idx_id),
  FOREIGN KEY(config_id)
    REFERENCES fds.IndexConfiguration(config_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

--importante note: we use the standard notation of NUMERIC for postgres and oracle! it is not compatible with the mysql and other DBMS!
CREATE TABLE fds.Execution (
  pe_id SERIAL,
  idx_id INTEGER NOT NULL,
  execution_name VARCHAR NOT NULL DEFAULT 'Execution',
  total_time NUMERIC NOT NULL,
  index_time NUMERIC NULL,
  filter_time NUMERIC NULL,
  refinement_time NUMERIC NULL,
  retrieving_objects_time NUMERIC NULL,
  processing_predicates_time NUMERIC NULL,
  read_time NUMERIC NULL,
  write_time NUMERIC NULL,
  split_time NUMERIC NULL,
  total_cpu_time NUMERIC NULL,
  index_cpu_time NUMERIC NULL,
  filter_cpu_time NUMERIC NULL,
  refinement_cpu_time NUMERIC NULL,
  retrieving_objects_cpu_time NUMERIC NULL,
  processing_predicates_cpu_time NUMERIC NULL,
  read_cpu_time NUMERIC NULL,
  write_cpu_time NUMERIC NULL,
  split_cpu_time NUMERIC NULL,
  processed_entries_num NUMERIC NULL,
  reinsertion_num INTEGER NULL,
  cand_num INTEGER NULL,
  result_num INTEGER NULL,
  writes_num INTEGER NULL,
  reads_num INTEGER NULL,
  split_int_num INTEGER NULL,
  split_leaf_num INTEGER NULL,
  query_predicate VARCHAR NULL,
  flushing_time NUMERIC NULL,
  flushing_cpu_time NUMERIC NULL,
  flushing_num INTEGER NULL,
  nof_unnecessary_flushed_nodes INTEGER NULL,
  visited_int_nodes_num INTEGER NULL,
  visited_leaf_nodes_num INTEGER NULL,
  written_int_nodes_num INTEGER NULL,
  written_leaf_nodes_num INTEGER NULL,
  deleted_int_nodes_num INTEGER NULL,
  deleted_leaf_nodes_num INTEGER NULL,
  mod_node_buffer_num INTEGER NULL,
  new_node_buffer_num INTEGER NULL,
  del_node_buffer_num INTEGER NULL,
  cur_new_node_buffer_num INTEGER NULL,
  cur_mod_node_buffer_num INTEGER NULL,
  cur_del_node_buffer_num INTEGER NULL,
  cur_buffer_size INTEGER NULL,
  ret_node_from_buf_time NUMERIC NULL,
  ret_node_from_buf_cpu_time NUMERIC NULL,
  write_log_time NUMERIC NULL,
  write_log_cpu_time NUMERIC NULL,
  compaction_log_time NUMERIC NULL,
  compaction_log_cpu_time NUMERIC NULL,
  recovery_log_time NUMERIC NULL,
  recovery_log_cpu_time NUMERIC NULL,
  compaction_log_num INTEGER NULL,
  writes_log_num INTEGER NULL,
  cur_log_size INTEGER NULL,
  flushed_nodes_num INTEGER NULL,
  merge_back_num INTEGER NULL,
  mods_pheight JSONB NULL,
  accesses_pheight JSONB NULL,
  std_buffer_page_fault INTEGER NULL,
  std_buffer_page_hit INTEGER NULL,
  std_buffer_find_time NUMERIC NULL,
  std_buffer_find_cpu_time NUMERIC NULL,
  std_buffer_flushing_time NUMERIC NULL,
  std_buffer_flushing_cpu_time NUMERIC NULL,
  read_buffer_page_hit INTEGER NULL,
  read_buffer_page_fault INTEGER NULL,
  cur_read_buffer_size INTEGER NULL,
  read_buffer_put_node_cpu_time NUMERIC NULL,
  read_buffer_put_node_time NUMERIC NULL,
  read_buffer_get_node_cpu_time NUMERIC NULL,
  read_buffer_get_node_time NUMERIC NULL,
  efind_force_node_in_rbuffer INTEGER NULL,
  efind_write_tc_sequential INTEGER NULL,
  efind_write_tc_stride INTEGER NULL,
  efind_write_tc_seqstride INTEGER NULL,
  efind_write_tc_filled INTEGER NULL,
  PRIMARY KEY(pe_id),
  FOREIGN KEY(idx_id)
    REFERENCES fds.SpatialIndex(idx_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.IndexSnapshot (
  pe_id INTEGER NOT NULL,
  height INTEGER NULL,
  num_int_nodes INTEGER NULL,
  num_leaf_nodes INTEGER NULL,
  num_entries_int_nodes INTEGER NULL,
  num_entries_leaf_nodes INTEGER NULL,
  num_nodes_pheight JSONB NULL,
  num_entries_pnode JSONB NULL,
  coverage_area_pnode JSONB NULL,
  overlap_area_pnode JSONB NULL,
  dead_space_pnode JSONB NULL,
  avg_num_entries_pnode NUMERIC NULL,
  avg_coverage_area_pnode NUMERIC NULL,
  avg_overlap_area_pnode NUMERIC NULL,
  avg_dead_space_pnode NUMERIC NULL,
  num_int_o_nodes INTEGER NULL,
  num_leaf_o_nodes INTEGER NULL,
  num_entries_int_o_nodes INTEGER NULL,
  num_entries_leaf_o_nodes INTEGER NULL,
  PRIMARY KEY(pe_id),
  FOREIGN KEY(pe_id)
    REFERENCES fds.Execution(pe_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.PrintIndex (
  pe_id INTEGER NOT NULL,
  node_id INTEGER NOT NULL,
  geom GEOMETRY NOT NULL,
  elem_position INTEGER NULL,
  o_node BOOL NULL,
  node_height INTEGER NULL,
  hilbert_value BIGINT NULL, --Only valid for Hilbert R-tree indices
  parent_node INTEGER NULL,
  FOREIGN KEY(pe_id)
    REFERENCES fds.Execution(pe_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.ReadWriteOrder (
  pe_id INTEGER NOT NULL,
  rw_order SERIAL,
  op_type VARCHAR NOT NULL,
  op_timestamp TIMESTAMP NOT NULL,
  page_id INTEGER NOT NULL,
  PRIMARY KEY(pe_id, rw_order),
  FOREIGN KEY(pe_id)
    REFERENCES fds.Execution(pe_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);

CREATE TABLE fds.FlashSimulatorStatistics (
  pe_id INTEGER NOT NULL,
  read_count BIGINT NOT NULL,
  write_count BIGINT NOT NULL,
  erase_count BIGINT NOT NULL,
  read_latency INTEGER NOT NULL,
  write_latency INTEGER NOT NULL,
  erase_latency INTEGER NOT NULL,
  PRIMARY KEY(pe_id),
  FOREIGN KEY(pe_id)
    REFERENCES fds.Execution(pe_id)
      ON DELETE NO ACTION
      ON UPDATE NO ACTION
);


----------------------------------------------------------------------------------------
------------------------------ GENERAL OPERATIONS --------------------------------------
----------------------------------------------------------------------------------------

CREATE OR REPLACE FUNCTION FT_CreateEmptySpatialIndex(index_id int4, index_name text, index_path text, src_id int4, bc_id int4, sc_id int4, buf_id int4)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_create_empty_index'
	LANGUAGE 'c' VOLATILE;

CREATE OR REPLACE FUNCTION FT_CreateEmptySpatialIndex(index_id int4, absolute_path text, src_id int4, bc_id int4, sc_id int4, buf_id int4)
	RETURNS bool AS 
$$
	SELECT FT_CreateEmptySpatialIndex(index_id, REGEXP_REPLACE(absolute_path, '.*/', ''), 
	substr(absolute_path, 0, char_length(absolute_path) - char_length(REGEXP_REPLACE(absolute_path, '.*/', '')) + 1), 
	src_id, bc_id, sc_id, buf_id) 
$$ 
LANGUAGE SQL;

CREATE TYPE __query_result AS (id integer, geo geometry);

-------------------------------------------------------------------------------
------------------ INSERTION, DELETION, AND UPDATE ----------------------------
-------------------------------------------------------------------------------

CREATE OR REPLACE FUNCTION FT_Insert(index_name text, index_path text, p int4, geom geometry)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_insert_entry'
	LANGUAGE 'c' VOLATILE STRICT;

CREATE OR REPLACE FUNCTION FT_Delete(index_name text, index_path text, p int4, geom geometry)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_remove_entry'
	LANGUAGE 'c' VOLATILE STRICT;

CREATE OR REPLACE FUNCTION FT_Update(index_name text, index_path text, old_p int4, old_geom geometry, new_p int4, new_geom geometry)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_update_entry'
	LANGUAGE 'c' VOLATILE STRICT;

CREATE OR REPLACE FUNCTION FT_Insert(absolute_path text, p int4, geom geometry)
	RETURNS bool AS
$$
	SELECT FT_Insert(REGEXP_REPLACE(absolute_path, '.*/', ''), 
	substr(absolute_path, 0, char_length(absolute_path) - char_length(REGEXP_REPLACE(absolute_path, '.*/', '')) + 1), 
	p, geom)
$$ 
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION FT_Delete(absolute_path text, p int4, geom geometry)
	RETURNS bool AS
$$
	SELECT FT_Delete(REGEXP_REPLACE(absolute_path, '.*/', ''), 
	substr(absolute_path, 0, char_length(absolute_path) - char_length(REGEXP_REPLACE(absolute_path, '.*/', '')) + 1), 
	p, geom)
$$ 
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION FT_Update(absolute_path text, old_p int4, old_geom geometry, new_p int4, new_geom geometry)
	RETURNS bool AS
$$
	SELECT FT_Update(REGEXP_REPLACE(absolute_path, '.*/', ''), 
	substr(absolute_path, 0, char_length(absolute_path) - char_length(REGEXP_REPLACE(absolute_path, '.*/', '')) + 1), 
	old_p, old_geom, new_p, new_geom)
$$ 
LANGUAGE SQL;

------------------------------------------------------------------
-------------------------- QUERYING AN INDEX ---------------------
------------------------------------------------------------------
--processing option = 1 means that the refinement and filter step will be processed. processing option = 2 means that only the filter step will be processed
CREATE OR REPLACE FUNCTION FT_QuerySpatialIndex(index_name text, index_path text, type_query int4, obj geometry, predicate int4, processing_option int4 default 1)
	RETURNS SETOF __query_result
	AS 'MODULE_PATHNAME', 'STI_query_spatial_index'
	LANGUAGE 'c' VOLATILE STRICT;

CREATE OR REPLACE FUNCTION FT_QuerySpatialIndex(absolute_path text, type_query int4, obj geometry, predicate int4, processing_option int4 default 1)
	RETURNS SETOF __query_result AS
$$
	SELECT FT_QuerySpatialIndex(REGEXP_REPLACE(absolute_path, '.*/', ''), 
	substr(absolute_path, 0, char_length(absolute_path) - char_length(REGEXP_REPLACE(absolute_path, '.*/', '')) + 1), 
	type_query, obj, predicate, processing_option)
$$ 
LANGUAGE SQL;

------------------------------------------------------------------------------------------
-------------------------- APPLYING ALL MODIFICATIONS IN THE BUFFER ---------------------
------------------------------------------------------------------------------------------

--this function apply all the modifications stored in the main memory buffer 
--it can be done only for flash-aware indices (FAI)
CREATE OR REPLACE FUNCTION FT_ApplyAllModificationsForFAI(index_name text, index_path text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_finish_fai'
	LANGUAGE 'c' VOLATILE STRICT;

--this function apply all the modifications stored in the main memory standard buffer
--that is, buffers specified in the BufferConfiguration table (e.g., LRU)
CREATE OR REPLACE FUNCTION FT_ApplyAllModificationsFromBuffer(index_name text, index_path text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_finish_buffer'
	LANGUAGE 'c' VOLATILE STRICT;

CREATE OR REPLACE FUNCTION FT_ApplyAllModificationsForFAI(absolute_path text)
	RETURNS bool AS
$$
	SELECT FT_ApplyAllModificationsForFAI(REGEXP_REPLACE(absolute_path, '.*/', ''), 
	substr(absolute_path, 0, char_length(absolute_path) - char_length(REGEXP_REPLACE(absolute_path, '.*/', '')) + 1))
$$ 
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION FT_ApplyAllModificationsFromBuffer(absolute_path text)
	RETURNS bool AS
$$
	SELECT FT_ApplyAllModificationsFromBuffer(REGEXP_REPLACE(absolute_path, '.*/', ''), 
	substr(absolute_path, 0, char_length(absolute_path) - char_length(REGEXP_REPLACE(absolute_path, '.*/', '')) + 1))
$$ 
LANGUAGE SQL;

----------------------------------------------------------------------------------------------
----------------------------- AUXILIARY OPERATIONS -------------------------------------------
----------------------------------------------------------------------------------------------

-------------------------------------------------------------------------
-- FUNCTIONS RELATED TO THE COLLECTION AND STORAGE OF STATISTICAL DATA --
-------------------------------------------------------------------------

CREATE OR REPLACE FUNCTION FT_StartCollectStatistics(read_write_order int4 default 0)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_start_collect_statistical_data'
	LANGUAGE 'c' VOLATILE STRICT;

CREATE OR REPLACE FUNCTION FT_CollectOrderOfReadWrite()
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_collect_read_write_order'
	LANGUAGE 'c' VOLATILE STRICT;

--return the execution_id generated
CREATE OR REPLACE FUNCTION FT_StoreStatisticalData(absolute_path text, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS int4 AS
$$
	SELECT FT_StoreStatisticalData(REGEXP_REPLACE(absolute_path, '.*/', ''), 
	substr(absolute_path, 0, char_length(absolute_path) - char_length(REGEXP_REPLACE(absolute_path, '.*/', '')) + 1),
	statistic_options, location_statistics, file_statistics)
$$ 
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION FT_StoreStatisticalData(index_name text, index_path text, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS int4
	AS 'MODULE_PATHNAME', 'STI_store_collected_statistical_data'
	LANGUAGE 'c' VOLATILE;

CREATE OR REPLACE FUNCTION FT_StoreIndexSnapshot(absolute_path text, execution_id int4, print_index bool default false, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS int4 AS
$$
	SELECT FT_StoreIndexSnapshot(REGEXP_REPLACE(absolute_path, '.*/', ''), 
	substr(absolute_path, 0, char_length(absolute_path) - char_length(REGEXP_REPLACE(absolute_path, '.*/', '')) + 1),
	execution_id, print_index, location_statistics, file_statistics)
$$ 
LANGUAGE SQL;

CREATE OR REPLACE FUNCTION FT_StoreIndexSnapshot(index_name text, index_path text, execution_id int4, print_index bool default false, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_store_index_snapshot'
	LANGUAGE 'c' VOLATILE;

CREATE OR REPLACE FUNCTION FT_SetExecutionName(execution_name text, location_statistics int4 default 1)
	RETURNS bool AS
$BODY$
BEGIN
	IF location_statistics = 1 THEN
		EXECUTE 'alter table fds.execution alter column execution_name set default ' || quote_literal(execution_name);
	ELSE
		PERFORM _FT_SetExecutionNameInMemory(execution_name);
	END IF;
	RETURN true;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

------------------ FUNCTIONS THAT HELPS US TO STORE STATISTICAL DATA IN SQL FILES --

CREATE OR REPLACE FUNCTION _FT_SetExecutionNameInMemory(execution_name text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'STI_set_execution_name'
	LANGUAGE 'c' VOLATILE;

CREATE OR REPLACE FUNCTION _FT_ProcessStatisticSpatialIndex(index_name text, index_path text, src_id int4, bc_id int4, sc_id int4, buf_id int4)
	RETURNS integer AS
$BODY$
DECLARE
	conf INTEGER;
	idx INTEGER;
BEGIN
	EXECUTE 'SELECT config_id, count(*) FROM fds.indexconfiguration WHERE src_id = $1 AND bc_id = $2 AND sc_id = $3 AND buf_id = $4 GROUP BY 1'
		INTO conf
		USING src_id, bc_id, sc_id, buf_id;

	IF conf IS NULL THEN
		EXECUTE 'INSERT INTO fds.indexconfiguration(src_id, bc_id, sc_id, buf_id) VALUES ($1, $2, $3, $4) RETURNING config_id'
			INTO conf
			USING src_id, bc_id, sc_id, buf_id;
	END IF;

	EXECUTE 'SELECT idx_id, count(*) FROM fds.spatialindex WHERE config_id = $1 AND idx_name = $2 AND idx_path = $3 GROUP BY 1'
		INTO idx
		USING conf, index_name, index_path;

	IF idx IS NULL THEN
		EXECUTE 'INSERT INTO fds.spatialindex(config_id, idx_name, idx_path, idx_creation, idx_last_mod) VALUES ($1, $2, $3, now(), now()) RETURNING idx_id'
			INTO idx
			USING conf, index_name, index_path;
	END IF;

	EXECUTE 'UPDATE fds.spatialindex SET idx_last_mod = now() WHERE idx_id = $1'
		USING idx;

	RETURN idx;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

----------------------------------------------------------------------------------------------
----------------------------- ATOMIC OPERATIONS -------------------------------------------
----------------------------------------------------------------------------------------------

-----------------------------------------------------------------------------------------
-------------------------- QUERYING AN INDEX AS AN ATOMIC OPERATION ---------------------
-----------------------------------------------------------------------------------------

CREATE OR REPLACE FUNCTION FT_AQuerySpatialIndex(index_name text, index_path text, type_query int4, obj geometry, predicate int4, processing_option int4 default 1, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS SETOF __query_result AS
$BODY$
BEGIN
	--we start to collect the statistical data of the related index
	PERFORM FT_StartCollectStatistics();
	--we perform the spatial query
	RETURN QUERY SELECT * FROM FT_QuerySpatialIndex(index_name, index_path, type_query, obj, predicate, processing_option);
	--we collect and store all the statistical data related to the spatial index creation
	PERFORM FT_StoreStatisticalData(index_name, index_path, statistic_options, location_statistics, file_statistics);
	
	RETURN;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

CREATE OR REPLACE FUNCTION FT_AQuerySpatialIndex(absolute_path text, type_query int4, obj geometry, predicate int4, processing_option int4 default 1, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS SETOF __query_result AS
$BODY$
BEGIN
	--we start to collect the statistical data of the related index
	PERFORM FT_StartCollectStatistics();
	--we perform the spatial query
	RETURN QUERY SELECT * FROM FT_QuerySpatialIndex(absolute_path, type_query, obj, predicate, processing_option);
	--we collect and store all the statistical data related to the spatial index creation
	PERFORM FT_StoreStatisticalData(absolute_path, statistic_options, location_statistics, file_statistics);
	
	RETURN;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

----------------------------------------------------------------------------------------------------
------------------ INSERTION, DELETION, AND UPDATE AS ATOMIC OPERATIONS ----------------------------
------- THESE FUNCTIONS RETURN THE EXECUTION_ID GENERATED BY THE FT_STORESTATISTICDATA -------------
----------------------------------------------------------------------------------------------------

CREATE OR REPLACE FUNCTION FT_AInsert(absolute_path text, p int4, geom geometry, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS int4 AS
$BODY$
BEGIN
	--we start to collect the statistical data of the related index
	PERFORM FT_StartCollectStatistics();
	--we perform the insert
	PERFORM FT_Insert(absolute_path, p, geom);
	--we collect and store all the statistical data related to the spatial index creation
	RETURN FT_StoreStatisticalData(absolute_path, statistic_options, location_statistics, file_statistics);
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

CREATE OR REPLACE FUNCTION FT_AInsert(index_name text, index_path text, p int4, geom geometry, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS int4 AS
$BODY$
BEGIN
	--we start to collect the statistical data of the related index
	PERFORM FT_StartCollectStatistics();
	--we perform the insert
	PERFORM FT_Insert(index_name, index_path, p, geom);
	--we collect and store all the statistical data related to the spatial index creation
	RETURN FT_StoreStatisticalData(index_name, index_path, statistic_options, location_statistics, file_statistics);
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

CREATE OR REPLACE FUNCTION FT_ADelete(absolute_path text, p int4, geom geometry, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS int4 AS
$BODY$
BEGIN
	--we start to collect the statistical data of the related index
	PERFORM FT_StartCollectStatistics();
	--we perform the remove
	PERFORM FT_Delete(absolute_path, p, geom);	
	--we collect and store all the statistical data related to the spatial index creation
	RETURN FT_StoreStatisticalData(absolute_path, statistic_options, location_statistics, file_statistics);
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

CREATE OR REPLACE FUNCTION FT_ADelete(index_name text, index_path text, p int4, geom geometry, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS int4 AS
$BODY$
BEGIN
	--we start to collect the statistical data of the related index
	PERFORM FT_StartCollectStatistics();
	--we perform the remove
	PERFORM FT_Delete(index_name, index_path, p, geom);	
	--we collect and store all the statistical data related to the spatial index creation
	RETURN FT_StoreStatisticalData(index_name, index_path, statistic_options, location_statistics, file_statistics);
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

CREATE OR REPLACE FUNCTION FT_AUpdate(absolute_path text, old_p int4, old_geom geometry, new_p int4, new_geom geometry, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS int4 AS
$BODY$
BEGIN
	--we start to collect the statistical data of the related index
	PERFORM FT_StartCollectStatistics();
	--we perform the update
	PERFORM FT_Update(absolute_path, old_p, old_geom, new_p, new_geom);
	--we collect and store all the statistical data related to the spatial index creation
	RETURN FT_StoreStatisticalData(absolute_path, statistic_options, location_statistics, file_statistics);
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

CREATE OR REPLACE FUNCTION FT_AUpdate(index_name text, index_path text, old_p int4, old_geom geometry, new_p int4, new_geom geometry, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS int4 AS
$BODY$
BEGIN
	--we start to collect the statistical data of the related index
	PERFORM FT_StartCollectStatistics();
	--we perform the update
	PERFORM FT_Update(index_name, index_path, old_p, old_geom, new_p, new_geom);
	--we collect and store all the statistical data related to the spatial index creation
	RETURN FT_StoreStatisticalData(index_name, index_path, statistic_options, location_statistics, file_statistics);
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;


------------------------------------------------------------------------------------------------
------------------ DEFAULT WORKLOAD: CREATION OF SPATIAL INDEX ---------------------------------
------------------------------------------------------------------------------------------------

--index_id is the type of the index (Integer values from 1 to 10 to respectively represent the R-tree, the R*-tree, the Hilbert R-tree, the FAST R-tree, the FAST R*-tree, the FAST Hilbert R-tree, the FOR-tree, the eFIND R-tree, the eFIND R*-tree, and the eFIND Hilbert R-tree)
--index_name is the name of the index
--index_path is the directory of the index
--src_id is the identifier of the source table from fds schema
--bc_id is the identifier of the basicconfiguration table from fds schema
--sc_id is the identifier of the specificconfiguration table from fds schema
--statistic_options is an identifiers in order to know the kind of statistical data to be collect and store (1 for collect and store only data for Execution table, 2 for collect and store data for Execution and IndexSnapshot, 3 for collect and store data for Execution and PrintIndex, and 4 for collect and store data for Execution, IndexSnapshot, and PrintIndex)
--apply_fai is a boolean value to specify if we will apply all the modifications contained in the buffer in main memory for flash-aware indices
--apply_stdbuffer is a boolean value to specify if we will apply all the modifications contained in the STANDARD buffer in main memory for all spatial indices (e.g., a spatial index that can use the LRU buffer)
CREATE OR REPLACE FUNCTION FT_CreateSpatialIndex(index_id int4, absolute_path text, src_id int4, bc_id int4, sc_id int4, buf_id int4, apply_fai bool default false, apply_stdbuffer bool default false, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS bool AS 
$BODY$
DECLARE
	src REFCURSOR;
	i INTEGER;
	sch VARCHAR;
	tab VARCHAR;
	colu VARCHAR;
	pk VARCHAR;	
	total INTEGER := 0;	
	rec RECORD;	
BEGIN
	EXECUTE 'SELECT schema_name, table_name, column_name, pk_name FROM fds.source WHERE src_id = $1'
		INTO sch, tab, colu, pk 
		USING src_id;

	--TODO checking if the parameters really exist in the FESTIval's data schema
	
	EXECUTE 'SELECT count(*) from ' || sch || '.' || tab 
		INTO total;	

	--we start to collect the statistical data of the related index
	PERFORM FT_StartCollectStatistics();

	--we create the empty new index
	PERFORM FT_CreateEmptySpatialIndex(index_id, absolute_path, src_id, bc_id, sc_id, buf_id);

	i := 0;
	WHILE (i <= total) LOOP
		OPEN src FOR 
			EXECUTE 'SELECT ' || pk || ' as pk,' || colu || ' as geom ' ||
				'FROM ' || sch || '.' || tab || ' ORDER BY ' || pk || ' LIMIT 100000 OFFSET $1' 
					USING i;
		LOOP
			FETCH src INTO rec;
			--check if we have more entries to be inserted
			EXIT WHEN NOT FOUND;
			--we insert entry by entry here
			PERFORM FT_Insert(absolute_path, rec.pk, rec.geom);
		END LOOP;
		CLOSE src;
		i := i + 100000;
	END LOOP;

	-- this is applied only for FAST and FOR-tree indices, which are flash-aware indices (FAI)
	IF (apply_fai AND (index_id = 4 OR index_id = 5 OR index_id = 6 OR index_id = 7 OR index_id = 8 OR index_id = 9 OR index_id = 10)) THEN
		PERFORM FT_ApplyAllModificationsForFAI(absolute_path);
	END IF;
	
	--we then apply all the modifications contained in the standard buffer
	IF (apply_stdbuffer) THEN
		PERFORM FT_ApplyAllModificationsFromBuffer(absolute_path);
	END IF;

	--we collect and store all the statistical data related to the spatial index creation
	PERFORM FT_StoreStatisticalData(absolute_path, statistic_options, location_statistics, file_statistics);

	RETURN true;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 500;

CREATE OR REPLACE FUNCTION FT_CreateSpatialIndex(index_id int4, index_name text, index_path text, src_id int4, bc_id int4, sc_id int4, buf_id int4, apply_fai bool default false, apply_stdbuffer bool default false, statistic_options int4 default 1, location_statistics int4 default 1, file_statistics text default NULL)
	RETURNS bool AS
$$
	SELECT FT_CreateSpatialIndex(index_id, index_path || index_name, src_id, bc_id, sc_id, buf_id, apply_fai, apply_stdbuffer, statistic_options, location_statistics, file_statistics)
$$ 
LANGUAGE SQL;
