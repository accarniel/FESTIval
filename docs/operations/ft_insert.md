# FT_Insert

## Summary

==FT_Insert== makes the insertion of a spatial object together with its primary key value in a spatial index. It returns `true` if the insertion is successfully performed and `false` otherwise.

## Signatures

Bool <span class="function">FT_Insert</span>(text <span class="param">index_name</span>, text <span class="param">index_directory</span>, integer <span class="param">pointer</span>, Geometry <span class="param">geom</span>);

Bool <span class="function">FT_Insert</span>(text <span class="param">apath</span>, integer <span class="param">pointer</span>, Geometry <span class="param">geom</span>);

## Description

==FT_Insert== makes the insertion of a spatial object together with its primary key value in a spatial index. It returns `true` if the insertion is successfully performed and `false` otherwise. 

!!! note
	==FT_Insert== does not automatically collect statistical data of the insertion. To do this collection, make use of its equivalent atomic operation or construct workloads with [auxiliary operations](../overview/#auxiliary_operations).

==FT_Insert== has two versions and its parameters are:

* <span class="param">index_name</span> is the name of the index file.
* <span class="param">index_directory</span> is the directory path that stores the index file.
* <span class="param">apath</span> is the absolute path of the index file.
* <span class="param">pointer</span> is the primary key value of the spatial object to be inserted. That is, the unique identifier that provides direct access to the register storing the spatial object.
* <span class="param">geom</span> is the spatial object (i.e., a PostGIS object) to be inserted. It can be an object of any type or format.

!!! danger "Caution"
	 The connected user of the database must be permission to read and write in the directory storing the index file. Otherwise, an error is returned.

!!! warning
	* <span class="param">pointer</span> should not be previously used by an indexed spatial object. Otherwise, spatial queries on this spatial index might return duplicate results.
	* It is important to keep the correspondence between the spatial index and its underlying spatial dataset. Hence, make sure that every indexed spatial object also exists in its underlying spatial dataset. This kind of control is out of scope of FESTIval.

## Examples

``` SQL
--using the first version of FT_Insert to insert a rectangular shaped object
SELECT FT_Insert('r-tree', '/opt/festival_indices/', 50, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857)
	);

--using the second version of FT_Insert to insert a spatial object into a spatial index. The spatial object is obtained from the underlying spatial dataset of the spatial index
SELECT FT_Insert('/opt/festival_indices/rstartree-brazil_points2017', 1000, 
	(SELECT way FROM brazil_points2017 WHERE id = 1000)
	);
```

## See Also

* The atomic version of FT_Insert - [FT_AInsert](../ft_ainsert)
* Constructing an empty spatial index is the first step before manipulating it - [FT_CreateEmptySpatialIndex](../ft_createemptyspatialindex)
* Creating workload - [see the FT_Insert being used in a workload](../../workloads/ft_createspatialindex)
