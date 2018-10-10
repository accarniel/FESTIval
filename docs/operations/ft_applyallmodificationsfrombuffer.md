# FT_ApplyAllModificationsFromBuffer

## Summary

==FT_ApplyAllModificationsFromBuffer== applies all the modifications stored in the general-purpose in-memory buffer (e.g., LRU) of the index spatial, if any. It returns `true` if the operation is successfully performed and `false` otherwise.

## Signature

Bool <span class="function">FT_ApplyAllModificationsFromBuffer</span>(text <span class="param">index_name</span>, text <span class="param">index_path</span>);

Bool <span class="function">FT_ApplyAllModificationsFromBuffer</span>(text <span class="param">apath</span>);

## Description

==FT_ApplyAllModificationsFromBuffer== applies all the modifications stored in the general-purpose in-memory buffer (e.g., LRU) of the index spatial, if any. It returns `true` if the operation is successfully performed and `false` otherwise.

==FT_ApplyAllModificationsFromBuffer== has two versions and its parameters are:

* <span class="param">index_name</span> is the name of the index file.
* <span class="param">index_directory</span> is the directory path that stores the index file.
* <span class="param">apath</span> is the absolute path of the index file.

!!! note
	* If the spatial index does not use a general-purpose buffer manager, ==FT_ApplyAllModificationsFromBuffer== does nothing and returns `true`.
	* The management of the general-purpose buffer includes flushing operations whenever the buffer is full. On the other hand, modifications can still be stored in the main memory after processing an index operation. Thus, before disconnecting from the database, this function could be called in order to apply the modifications that were not written to the storage device yet.
	* This function does not collect and store statistical data. However, collecting the order of reads and writes is possible (see [FT_CollectOrderOfReadWrite](../ft_collectorderofreadwrite)).

!!! danger "Caution"
	The connected user of the database must be permission to read and write in the directory storing the index file. Otherwise, an error is returned.

## Examples

``` SQL
--setting the execution name since we will now insert a new object
SELECT FT_SetExecutionName('a single insertion');

SELECT FT_AInsert('r-tree', '/opt/festival_indices/', 50, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857)
	);

--applying all modifications stored in the general-purpose buffer manager
SELECT FT_ApplyAllModificationsFromBuffer('r-tree', '/opt/festival_indices/');
```

## See Also

* Creating workload - [see the FT_ApplyAllModificationsFromBuffer being used in a workload](../../workloads/ft_createspatialindex)
