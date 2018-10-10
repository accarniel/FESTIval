# FT_StoreIndexSnapshot

## Summary

==FT_StoreIndexSnapshot== collects and stores statistical data related to the index structure. It returns `true` if the storing is successfully performed and `false` otherwise.

## Signatures

Bool <span class="function">FT_StoreIndexSnapshot</span>(text <span class="param">index_name</span>, text <span class="param">index_directory</span>, integer <span class="param">execution_id</span>, bool <span class="param">print_index=false</span>, integer <span class="param">loc_stat_data=1</span>, text <span class="param">file=NULL</span>);

Bool <span class="function">FT_StoreIndexSnapshot</span>(text <span class="param">apath</span>, integer <span class="param">execution_id</span>, bool <span class="param">print_index=false</span>, integer <span class="param">loc_stat_data=1</span>, text <span class="param">file=NULL</span>);

## Description

==FT_StoreIndexSnapshot== collects and stores statistical data related to the index structure. It returns `true` if the storing is successfully performed and `false` otherwise.

This operation has two versions and its parameters are:

* <span class="param">index_name</span> is the name of the index file.
* <span class="param">index_directory</span> is the directory path that stores the index file.
* <span class="param">apath</span> is the absolute path of the index file.
* <span class="param">execution_id</span> is the primary key value of the table Execution. The statistical data collected and stored by ==FT_StoreIndexSnapshot== is always associated to a tuple in the table Execution.
* <span class="param">print_index</span> is a flag that indicates if the nodes of the index should be also be stored. Its default is `false`, indicating that this collection should not be performed.
* <span class="param">loc_stat_data</span> defines where the statistical data should be stored. If its value is equal to ``1``, its default value, the statistical data is stored directly in the FESTIval’s data schema. If its value is equal to ``2``, the statistical data is stored in a SQL file that can be latter loaded into the FESTIval’s data schema.
* <span class="param">file</span> is the absolute path of the SQL file that will store the statistical data, if <span class="param">loc_stat_data</span> is equal to ``2``. This file will be created if it does not exist. This parameter is not used if <span class="param">loc_stat_data</span> is equal to ``1``.

!!! note
	We recommend that you see the [FESTIval's data schema](../../data_schema/overview) in order to understand the types of statistical data that can be managed.

!!! danger "Caution"
	If <span class="param">loc_stat_data</span> is equal to ``2``, the connected user of the database must be permission to write in the directory storing this SQL file. Otherwise, an error is returned.

## Examples

``` SQL
--setting the execution name
SELECT FT_SetExecutionName('a workload focused on queries');

--performing the operations
SELECT FT_AInsert('/opt/festival_indices/rstartree-brazil_points2017', 1000, 
	(SELECT way FROM brazil_points2017 WHERE id = 1000)
	);
SELECT FT_AInsert('/opt/festival_indices/rstartree-brazil_points2017', 1001, 
	(SELECT way FROM brazil_points2017 WHERE id = 1001)
	);
SELECT FT_AInsert('/opt/festival_indices/rstartree-brazil_points2017', 1002, 
	(SELECT way FROM brazil_points2017 WHERE id = 1002)
	);
--queries could be performed here

--storing statistical data related to the index structure to analyze the impact of the last insertion on the structure of the index
SELECT FT_StoreIndexSnapshot('/opt/festival_indices/r-tree', 
	(SELECT max(pe_id) FROM fds.execution), 
	false, 2, '/opt/statistical_data.sql');
```

## See Also

* Collecting statistical data with [FT_StartCollectStatistics](../ft_startcollectstatistics)
* Storing statistical data with [FT_StoreStatisticalData](../ft_storestatisticaldata)