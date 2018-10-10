# FT_Update

## Summary

==FT_Update== makes the update of a spatial object indexed in a spatial index. It returns `true` if the update is successfully performed and `false` otherwise.

## Signatures

Bool <span class="function">FT_Update</span>(text <span class="param">index_name</span>, text <span class="param">index_directory</span>, integer <span class="param">old_pointer</span>, Geometry <span class="param">old_geom</span>, integer <span class="param">new_pointer</span>, Geometry <span class="param">new_geom</span>);

Bool <span class="function">FT_Update</span>(text <span class="param">apath</span>, integer <span class="param">old_pointer</span>, Geometry <span class="param">old_geom</span>, integer <span class="param">new_pointer</span>, Geometry <span class="param">new_geom</span>);

## Description

==FT_Update== makes the update of a spatial object indexed in a spatial index. It returns `true` if the update is successfully performed and `false` otherwise. 

!!! note
	* ==FT_Update== does not automatically collect statistical data of the update. To do this collection, make use of its equivalent atomic operation or construct workloads with [auxiliary operations](../overview/#auxiliary_operations).
	* In general, this operation sequentially deletes the old spatial object (and its pointer) and then inserts the new spatial object (and its pointer). Thus, if the old spatial object is not found in the spatial index, ==FT_Update== returns `false`.

==FT_Update== has two versions and its parameters are:

* <span class="param">index_name</span> is the name of the index file.
* <span class="param">index_directory</span> is the directory path that stores the index file.
* <span class="param">apath</span> is the absolute path of the index file.
* <span class="param">old_pointer</span> is the primary key value of the spatial object to be updated. That is, the unique identifier that provides direct access to the register storing the spatial object.
* <span class="param">old_geom</span> is the spatial object (i.e., a PostGIS object) to be updated.
* <span class="param">new_pointer</span> is the new primary key value of the spatial object being updated. 
* <span class="param">new_geom</span> is the new spatial object (i.e., a PostGIS object) that will replace the <span class="param">old_geom</span>.

!!! danger "Caution"
	 The connected user of the database must be permission to read and write in the directory storing the index file. Otherwise, an error is returned.

!!! warning
	* <span class="param">new_pointer</span> should not be previously used by an indexed spatial object. Otherwise, spatial queries on this spatial index might return duplicate results.
	* It is important to keep the correspondence between the spatial index and its underlying spatial dataset. Hence, make sure that every indexed spatial object also exists in its underlying spatial dataset (and that they are the same object). This kind of control is out of scope of FESTIval.

## Examples

``` SQL
--using the first version of FT_Update to update the rectangular shaped object of the FT_Insert example
--only the spatial object is being updated
SELECT FT_Update('r-tree', '/opt/festival_indices/', 50, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857), 50, 
	ST_GeomFromText('POLYGON((-6349100.26886151 -751965.038197354,-6349100.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349100.26886151 -751965.038197354))', 3857)
	);

--using the second version of FT_Update. The spatial object is obtained from the underlying spatial dataset of the spatial index. It is transformed by using the function st_buffer.
SELECT FT_Update('/opt/festival_indices/rstartree-brazil_points2017', 1000, 
	(SELECT way FROM brazil_points2017 WHERE id = 1000), 1000, 
	(SELECT ST_Buffer(way, 50) FROM brazil_points2017 WHERE id = 1000)
	);
```

## See Also

* The atomic version of FT_Update - [FT_AUpdate](../ft_aupdate)
* Constructing an empty spatial index is the first step before manipulating it - [FT_CreateEmptySpatialIndex](../ft_createemptyspatialindex)
* Functions to insert and delete spatial objects - [FT_Insert](../ft_insert) and [FT_Delete](../ft_delete)