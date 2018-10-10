# FT_AQuerySpatialIndex

## Summary

==FT_AQuerySpatialIndex== is the atomic version of ==FT_QuerySpatialIndex==. It executes a spatial query using a given spatial index, and collects and stores related statistical data. It returns a set of spatial objects that either corresponds to the filter step or refinement step (i.e., final result) of the spatial query processing.


## Signatures

setof <span class="param">query_result</span> <span class="function">FT_AQuerySpatialIndex</span>(text <span class="param">index_name</span>, text <span class="param">index_directory</span>, integer <span class="param">query_type</span>, Geometry <span class="param">search_obj</span>, integer <span class="param">predicate</span>, integer <span class="param">proc_option=1</span>, integer <span class="param">statistic_option=1</span>, integer <span class="param">loc_stat_data=1</span>, text <span class="param">file=NULL</span>);

setof <span class="param">query_result</span> <span class="function">FT_AQuerySpatialIndex</span>(text <span class="param">apath</span>, , integer <span class="param">query_type</span>, Geometry <span class="param">search_obj</span>, integer <span class="param">predicate</span>, integer <span class="param">proc_option=1</span>, integer <span class="param">statistic_option=1</span>, integer <span class="param">loc_stat_data=1</span>, text <span class="param">file=NULL</span>);

## Description

==FT_AQuerySpatialIndex== is the atomic version of ==FT_QuerySpatialIndex==. It executes a spatial query using a given spatial index, and collects and stores related statistical data. It returns a set of spatial objects that either corresponds to the filter step or refinement step (i.e., final result) of the spatial query processing.

==FT_AQuerySpatialIndex== is a set-returning function of the PostgreSQL. It returns <span class="param">query_result</span> rows, formed by a primary key value (**id**) and a spatial object (**geom**) of the indexed dataset. ==FT_AQuerySpatialIndex== has two versions and its parameters are:

* <span class="param">index_name</span> is the name of the index file.
* <span class="param">index_directory</span> is the directory path that stores the index file.
* <span class="param">apath</span> is the absolute path of the index file.
* <span class="param">query_type</span> is the type of spatial query to be processed. The value ``1`` corresponds to *selection query*, ``2`` corresponds to *range query*, and ``3`` corresponds to *point query*. Spatial selection is a general type of query that returns a set of spatial objects that satisfy some topological predicate (indicated by the paramerter <span class="param">predicate</span>) for a given spatial object (indicated by the parameter <span class="param">search_obj</span>). Range query specializes spatial selection by allowing rectangular-shaped search objects. Point query specializes spatial selection by allowing only the use of intersects as the topological predicate and points as search objects. More information regarding types of spatial queries is given in [Gaede and G&uuml;nther (1998)](#fn:1).
* <span class="param">search_obj</span> is the spatial object (i.e., a PostGIS object) corresponding to the search object of the spatial query.
* <span class="param">predicate</span> is the topological predicate to be used in the spatial query: ``1`` corresponds to ``Intersects``, ``2`` corresponds to ``Overlap``, ``3`` corresponds to ``Disjoint``, ``4`` corresponds to ``Meet``, ``5`` corresponds to ``Inside``, ``6`` corresponds to ``CoveredBy``, ``6`` corresponds to ``Contains``, ``7`` corresponds to ``Covers``, and ``8`` corresponds to ``Equals``. These predicates are processed according to the **9-Intersection Model** [(Schneider and Behr, 2006)](#fn:2).
* <span class="param">proc_option</span> refers to the type of the result of the spatial query. If it has the value equal to ``1``, ==FT_QuerySpatialIndex== returns the final result of the spatial query (i.e., process both filter and refinement steps, if needed [(Gaede and G&uuml;nther, 1998)](#fn:1)) If it has the value equal to ``2``, ==FT_QuerySpatialIndex== returns the candidates returned by the spatial index (i.e., the filter step results).
* <span class="param">statistic_option</span> refers to the type of statistical data to be collected and stored. If <span class="param">statistic_option</span> is equal to ``1``, its default value, FESTIval collects standard statistical data to be inserted as a new tuple in the table Execution. If <span class="param">statistic_option</span> is equal to ``2`` or ``4``, FESTIval collects statistical data related to the structure of the index to be inserted as a new tuple in the table IndexSnapshot. If <span class="param">statistic_option</span> is equal to ``3`` or ``4``, FESTIval collects the nodes of the index to be inserted as new tuples in the table PrintIndex. Note that the value ``4`` indicates, therefore, that all types of statistical data is collected and stored.
* <span class="param">loc_stat_data</span> defines where the statistical data should be stored. If its value is equal to ``1``, its default value, the statistical data is stored directly in the FESTIval’s data schema. If its value is equal to ``2``, the statistical data is stored in a SQL file that can be latter loaded into the FESTIval’s data schema.
* <span class="param">file</span> is the absolute path of the SQL file that will store the statistical data, if <span class="param">loc_stat_data</span> is equal to ``2``. This file will be created if it does not exist. This parameter is not used if <span class="param">loc_stat_data</span> is equal to ``1``.

[^1]: 
	V. Gaede, O. G&uuml;nther, Multidimensional access methods, ACM Computing Surveys 30 (2) (1998) 170–231.

[^2]: 
	M. Schneider, T. Behr, Topological relationships between complex spatial objects, ACM Transactions on Database Systems 31 (1) (2006) 39–81.

!!! note
	* Some restrictions with respect to the geometric format of <span class="param">search_obj</span> may be applicable. If <span class="param">query_type</span> is equal to ``2``, the bounding box of <span class="param">search_obj</span> is considered. If <span class="param">query_type</span> is equal to ``3``, <span class="param">search_obj</span> must be a point object.  
	* If <span class="param">proc_option</span> if equal to ``2``, the attribute **geom** of the <span class="param">query_result</span> is equal to ``null``.
	* We recommend that you see the [FESTIval's data schema](../../data_schema/overview) in order to understand the types of statistical data that can be managed.
	* If <span class="param">loc_stat_data</span> is equal to ``2``, the returning value is invalid since the insertion is not made directly on the table Execution. A valid treatment is performed on the file storing the statistical data.


!!! danger "Caution"
	 * The connected user of the database must be permission to read and write in the directory storing the index file. Otherwise, an error is returned.
	 * If <span class="param">loc_stat_data</span> is equal to ``2``, the connected user of the database must be permission to write in the directory storing this SQL file. Otherwise, an error is returned.

!!! warning
	It is important to keep the correspondence between the spatial index and its underlying spatial dataset. Hence, make sure that every indexed spatial object also exists in its underlying spatial dataset. This kind of control is out of scope of FESTIval.

## Examples

``` SQL
-- range query considering the predicate Overlap
select * from FT_AQuerySpatialIndex('r-tree', '/opt/festival_indices/', 2, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857), 2);

-- point query, returning only the ids of the returned objects
select id from FT_AQuerySpatialIndex('/opt/festival_indices/r-tree', 3, 
	ST_GeomFromText('POINT(-6349160.26886151 -751965.038197354)', 3857), 1, 1, 2, '/opt/statistical_data.sql');
```

## See Also

* The general version of FT_AQuerySpatialIndex - [FT_QuerySpatialIndex](../ft_queryspatialindex)
* Constructing an empty spatial index is the first step before manipulating it - [FT_CreateEmptySpatialIndex](../ft_createemptyspatialindex)