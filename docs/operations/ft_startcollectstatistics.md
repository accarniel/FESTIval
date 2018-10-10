# FT_StartCollectStatistics

## Summary

==FT_StartCollectStatistics== defines the moment in which FESTIval should collect statistical data from executed operations. It returns `true` if the setting is successfully performed and `false` otherwise.

## Signature

Bool <span class="function">FT_StartCollectStatistics</span>(bool <span class="param">rw=false</span>);

## Description

==FT_StartCollectStatistics== defines the moment in which FESTIval should collect statistical data from executed operations. It returns `true` if the setting is successfully performed and `false` otherwise.

* <span class="param">rw</span> is a boolean value indicating if the order of reads and writes should also be collected. Its default value is `false`.

After calling this function, statistical data related to the execution of the next index operations (e.g., insertion, deletion, update, and query) will be collected in the main memory. But, these data will be stored only when the following function is called: [FT_StoreStatisticalData](../ft_storestatisticaldata).

!!! note
	Statistical data collected in the main memory is automatically deallocated when the database is disconnected. 

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

* Creating workload - [see the FT_StartCollectStatistics being used in a workload](../../workloads/ft_createspatialindex)
* Storing statistical data with [FT_StoreStatisticalData](../ft_storestatisticaldata)