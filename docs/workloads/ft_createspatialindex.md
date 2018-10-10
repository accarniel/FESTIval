# FT_CreateSpatialIndex

## Summary

==FT_CreateSpatialIndex== builds a spatial index by inserting spatial objects stored in a spatial dataset. The elements are indexed one-by-one. It returns `true` if the spatial index is successfully constructed and `false` otherwise.

## Signature

Bool <span class="function">FT_CreateSpatialIndex</span>(integer <span class="param">index_id</span>, text <span class="param">apath</span>, integer <span class="param">src_id</span>, integer <span class="param">bc_id</span>, integer <span class="param">sc_id</span>, integer <span class="param">buf_id</span>, bool <span class="param">apply_fai=false</span>, bool <span class="param">apply_stdbuffer=false</span>, integer <span class="param">statistic_option=1</span>, integer <span class="param">location_stat_data=1</span>, text <span class="param">file=NULL</span>);


## Description

==FT_CreateSpatialIndex== builds a spatial index by inserting spatial objects stored in a spatial dataset. The elements are indexed one-by-one. It returns `true` if the spatial index is successfully constructed and `false` otherwise.

==FT_CreateSpatialIndex== is a workload that employs the following FESTIval operations: 

* [*FT_StartCollectStatistics*](../../operations/ft_startcollectstatistics).
* [*FT_CreateEmptySpatialIndex*](../../operations/FT_CreateEmptySpatialIndex).
* [*FT_Insert*](../../operations/FT_Insert).
* [*FT_ApplyAllModificationsForFAI*](../../operations/FT_ApplyAllModificationsForFAI).
* [*FT_ApplyAllModificationsFromBuffer*](../../operations/FT_ApplyAllModificationsFromBuffer).
* [*FT_StoreStatisticalData*](../../operations/FT_StoreStatisticalData).

!!! danger "Caution"
	 The connected user of the database must be permission to read and write in the directory storing the index file. Otherwise, an error is returned.

!!! warning
	It is important to keep the correspondence between the spatial index and its underlying spatial dataset. Hence, make sure that every indexed spatial object also exists in its underlying spatial dataset. This kind of control is out of scope of FESTIval.

==FT_CreateSpatialIndex== is defined as follows:

``` SQL
CREATE OR REPLACE FUNCTION FT_CreateSpatialIndex(index_id int4, apath text, src_id int4, bc_id int4, sc_id int4, buf_id int4, apply_fai bool default false, apply_stdbuffer bool default false, statistic_option int4 default 1, location_stat_data int4 default 1, file text default NULL)
	RETURNS bool AS 
$BODY$
	DECLARE
    	src REFCURSOR; rec RECORD;
      	i INTEGER; total INTEGER := 0;
      	sch VARCHAR; tab VARCHAR; colu VARCHAR; pk VARCHAR;
	BEGIN
		EXECUTE 'SELECT schema_name, table_name, column_name, pk_name 
				 FROM fds.source 
				 WHERE src id = $1' 
				 INTO sch, tab, colu, pk USING src id;
	    EXECUTE 'SELECT count(*) 
	    		 FROM ' || sch || '.' || tab 
	    		 INTO total;
		PERFORM FT_StartCollectStatistics();
		PERFORM FT_CreateEmptySpatialIndex(index_id, apath, src_id, bc_id, sc_id, buf_id);
		i:=0;
		WHILE (i <= total) LOOP
	    	OPEN src FOR EXECUTE 'SELECT ' || pk || ' as pk, ' || colu || ' as geom 
	    						  FROM ' || sch || '.' || tab || ' 
	    						  ORDER BY ' || pk || ' 
	    						  LIMIT 100000 OFFSET $1' 
	    						  USING i; LOOP
				FETCH src INTO rec;
				EXIT WHEN NOT FOUND;
				PERFORM FT_Insert(apath, rec.pk, rec.geom);
			END LOOP;
			CLOSE src; 
			i := i + 100000;
		END LOOP;
		IF (apply_fai AND (index_id = 4 OR index_id = 5 OR index_id = 6 OR index_id = 7 OR
			index_id = 8 OR index_id = 9 OR index_id = 10)) THEN 
			PERFORM FT_ApplyAllModificationsForFAI(apath);
		END IF;
		IF (apply_stdbuffer) THEN
			PERFORM FT_ApplyAllModificationsFromBuffer(apath); 
		END IF;
		PERFORM FT_StoreStatisticalData(apath, statistic_option, location_stat_data, file);
		RETURN true; 
	END
$BODY$
LANGUAGE plpgsql VOLATILE 
COST 500;
```

## Examples of usage

``` SQL
--first example:
SELECT FT_SetExecutionName('Creating R-tree over brazil_points2017');
SELECT FT_CreateSpatialIndex(1, '/opt/linear_rtree', 7, 6, 18, 4);

--second example:
SELECT FT_SetExecutionName('Creating R-tree over brazil_points2017');
SELECT FT_CreateSpatialIndex(1, '/opt/linear_rtree2', 7, 6, 18, 4, false, false, 4);
```
