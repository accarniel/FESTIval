# ST_CollectOrderOfReadWrite

## Summary

==ST_CollectOrderOfReadWrite== indicates that the order of reads and writes should be also collected. It returns `true` if the setting is successfully performed and `false` otherwise.

## Signature

Bool <span class="function">ST_CollectOrderOfReadWrite</span>();

## Description

==ST_CollectOrderOfReadWrite== indicates that the order of reads and writes should be also collected. It returns `true` if the setting is successfully performed and `false` otherwise.

After calling this function, the order of reads and writes stops to be collected after calling [FT_StoreStatisticalData](../ft_storestatisticaldata).

!!! note
	* Collecting the order of reads and writes may require extra computation time. Thus, we recommend the use of this function to analyze patterns of access in a storage device.
	* The order of reads and writes is collected in the main memory and is automatically deallocated when the database is disconnected. 
	* The order of reads and writes is not collected after calling an atomic operation because an atomic operation includes the execution of [FT_StoreStatisticalData](../ft_storestatisticaldata).


## Examples

``` SQL
--setting the execution name
SELECT FT_SetExecutionName('an insertion');

--let us store the order of reads and writes of the next atomic operation
SELECT ST_CollectOrderOfReadWrite();

--the order of reads and writes is only collected for this operation 
-- because this operation calls the FT_StoreStatisticalData, which stops the collection of the order of reads and writes
SELECT FT_AInsert('r-tree', '/opt/festival_indices/', 50, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857)
	);
```

## See Also

* Storing statistical data with [FT_StoreStatisticalData](../ft_storestatisticaldata)