# FT_AUpdate

## Summary

==FT_AUpdate== is the atomic version of ==FT_Update==. It makes the update of a spatial object indexed in a spatial index, and collects and stores related statistical data. It returns the primary key value that corresponds to the identifier of the row inserted into the table Execution.

## Signatures

integer <span class="function">FT_AUpdate</span>(text <span class="param">index_name</span>, text <span class="param">index_directory</span>, integer <span class="param">old_pointer</span>, Geometry <span class="param">old_geom</span>, integer <span class="param">new_pointer</span>, Geometry <span class="param">new_geom</span>, integer <span class="param">statistic_option=1</span>, integer <span class="param">loc_stat_data=1</span>, text <span class="param">file=NULL</span>);

integer <span class="function">FT_AUpdate</span>(text <span class="param">apath</span>, integer <span class="param">old_pointer</span>, Geometry <span class="param">old_geom</span>, integer <span class="param">new_pointer</span>, Geometry <span class="param">new_geom</span>, integer <span class="param">statistic_option=1</span>, integer <span class="param">loc_stat_data=1</span>, text <span class="param">file=NULL</span>);

## Description

==FT_AUpdate== is the atomic version of ==FT_Update==. It makes the update of a spatial object indexed in a spatial index, and collects and stores related statistical data. It returns the primary key value that corresponds to the identifier of the row inserted into the table Execution.

!!! note
	==FT_AUpdate== is atomic because it executes three SQL functions as a unique operation.

==FT_AUpdate== has two versions and its parameters are:

* <span class="param">index_name</span> is the name of the index file.
* <span class="param">index_directory</span> is the directory path that stores the index file.
* <span class="param">apath</span> is the absolute path of the index file.
* <span class="param">old_pointer</span> is the primary key value of the spatial object to be updated. That is, the unique identifier that provides direct access to the register storing the spatial object.
* <span class="param">old_geom</span> is the spatial object (i.e., a PostGIS object) to be updated.
* <span class="param">new_pointer</span> is the new primary key value of the spatial object being updated. 
* <span class="param">new_geom</span> is the new spatial object (i.e., a PostGIS object) that will replace the <span class="param">old_geom</span>.
* <span class="param">statistic_option</span> refers to the type of statistical data to be collected and stored. If <span class="param">statistic_option</span> is equal to ``1``, its default value, FESTIval collects standard statistical data to be inserted as a new tuple in the table Execution. If <span class="param">statistic_option</span> is equal to ``2`` or ``4``, FESTIval collects statistical data related to the structure of the index to be inserted as a new tuple in the table IndexSnapshot. If <span class="param">statistic_option</span> is equal to ``3`` or ``4``, FESTIval collects the nodes of the index to be inserted as new tuples in the table PrintIndex. Note that the value ``4`` indicates, therefore, that all types of statistical data is collected and stored.
* <span class="param">loc_stat_data</span> defines where the statistical data should be stored. If its value is equal to ``1``, its default value, the statistical data is stored directly in the FESTIval’s data schema. If its value is equal to ``2``, the statistical data is stored in a SQL file that can be latter loaded into the FESTIval’s data schema.
* <span class="param">file</span> is the absolute path of the SQL file that will store the statistical data, if <span class="param">loc_stat_data</span> is equal to ``2``. This file will be created if it does not exist. This parameter is not used if <span class="param">loc_stat_data</span> is equal to ``1``.

!!! note
	* We recommend that you see the [FESTIval's data schema](../../data_schema/overview) in order to understand the types of statistical data that can be managed.
	* If <span class="param">loc_stat_data</span> is equal to ``2``, the returning value is invalid since the insertion is not made directly on the table Execution. A valid treatment is performed on the file storing the statistical data.

!!! danger "Caution"
	 * The connected user of the database must be permission to read and write in the directory storing the index file. Otherwise, an error is returned.
	 * If <span class="param">loc_stat_data</span> is equal to ``2``, the connected user of the database must be permission to write in the directory storing this SQL file. Otherwise, an error is returned.

!!! warning
	* <span class="param">new_pointer</span> should not be previously used by an indexed spatial object. Otherwise, spatial queries on this spatial index might return duplicate results.
	* It is important to keep the correspondence between the spatial index and its underlying spatial dataset. Hence, make sure that every indexed spatial object also exists in its underlying spatial dataset. This kind of control is out of scope of FESTIval.

## Examples

``` SQL
--using the first version of FT_Update to update the rectangular shaped object of the FT_Insert example
--only the spatial object is being updated
SELECT FT_AUpdate('r-tree', '/opt/festival_indices/', 50, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857), 50, 
	ST_GeomFromText('POLYGON((-6349100.26886151 -751965.038197354,-6349100.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349100.26886151 -751965.038197354))', 3857)
	);

--using the second version of FT_Update. The spatial object is obtained from the underlying spatial dataset of the spatial index. It is transformed by using the function st_buffer.
SELECT FT_AUpdate('/opt/festival_indices/rstartree-brazil_points2017', 1000, 
	(SELECT way FROM brazil_points2017 WHERE id = 1000), 1000, 
	(SELECT ST_Buffer(way, 50) FROM brazil_points2017 WHERE id = 1000),
	4, 2, '/opt/statistical_data.sql'
	);
```

## See Also

* The general version of FT_AUpdate - [FT_Update](../ft_update)
* Constructing an empty spatial index is the first step before manipulating it - [FT_CreateEmptySpatialIndex](../ft_createemptyspatialindex)
* The atomic versions of functions that insert and delete spatial objects - [FT_AInsert](../ft_ainsert) and [FT_ADelete](../ft_adelete)
