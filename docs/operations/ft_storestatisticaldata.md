# FT_StoreStatisticalData

## Summary

==FT_StoreStatisticalData== stores the collected statistical data. It returns the primary key value that corresponds to the identifier of the row inserted into the table Execution.

## Signatures

integer <span class="function">FT_StoreStatisticalData</span>(text <span class="param">index_name</span>, text <span class="param">index_directory</span>, integer <span class="param">statistic_option=1</span>, integer <span class="param">loc_stat_data=1</span>, text <span class="param">file=NULL</span>);

integer <span class="function">FT_StoreStatisticalData</span>(text <span class="param">apath</span>, integer <span class="param">statistic_option=1</span>, integer <span class="param">loc_stat_data=1</span>, text <span class="param">file=NULL</span>);

## Description

==FT_StoreStatisticalData== stores the collected statistical data. It returns the primary key value that corresponds to the identifier of the row inserted into the table Execution.

This operation has two versions and its parameters are:

* <span class="param">index_name</span> is the name of the index file.
* <span class="param">index_directory</span> is the directory path that stores the index file.
* <span class="param">apath</span> is the absolute path of the index file.
* <span class="param">statistic_option</span> refers to the type of statistical data to be collected and stored. If <span class="param">statistic_option</span> is equal to ``1``, its default value, FESTIval collects standard statistical data to be inserted as a new tuple in the table Execution. If <span class="param">statistic_option</span> is equal to ``2`` or ``4``, FESTIval collects statistical data related to the structure of the index to be inserted as a new tuple in the table IndexSnapshot. If <span class="param">statistic_option</span> is equal to ``3`` or ``4``, FESTIval collects the nodes of the index to be inserted as new tuples in the table PrintIndex. Note that the value ``4`` indicates, therefore, that all types of statistical data is collected and stored.
* <span class="param">loc_stat_data</span> defines where the statistical data should be stored. If its value is equal to ``1``, its default value, the statistical data is stored directly in the FESTIval’s data schema. If its value is equal to ``2``, the statistical data is stored in a SQL file that can be latter loaded into the FESTIval’s data schema.
* <span class="param">file</span> is the absolute path of the SQL file that will store the statistical data, if <span class="param">loc_stat_data</span> is equal to ``2``. This file will be created if it does not exist. This parameter is not used if <span class="param">loc_stat_data</span> is equal to ``1``.

!!! note
	* We recommend that you see the [FESTIval's data schema](../../data_schema/overview) in order to understand the types of statistical data that can be managed.
	* If <span class="param">loc_stat_data</span> is equal to ``2``, the returning value is invalid since the insertion is not made directly on the table Execution. A valid treatment is performed on the file storing the statistical data.

!!! danger "Caution"
	If <span class="param">loc_stat_data</span> is equal to ``2``, the connected user of the database must be permission to write in the directory storing this SQL file. Otherwise, an error is returned.

## Examples

``` SQL
--setting the execution name
SELECT FT_SetExecutionName('an insertion followed by an update operation');

--start collecting statistical data
SELECT FT_StartCollectStatistics();

--performing the operations
SELECT FT_Insert('r-tree', '/opt/festival_indices/', 50, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857)
	);

SELECT FT_Update('/opt/festival_indices/rstartree-brazil_points2017', 50, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857),
	50, ST_Buffer(ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857), 10)
	);

--storing the collected statistical data
SELECT FT_StoreStatisticalData();
```

## See Also

* Creating workload - [see the FT_StoreStatisticalData being used in a workload](../../workloads/ft_createspatialindex)
* Collecting statistical data with [FT_StartCollectStatistics](../ft_startcollectstatistics)
