# FT_CreateEmptySpatialIndex

## Summary

==FT_CreateEmptySpatialIndex== initializes the structure of a spatial index. The created spatial index is empty; thus, it does not contain any indexed spatial object yet. It returns `true` if the operation is successfully performed and `false` otherwise.


## Signatures

Bool <span class="function">FT_CreateEmptySpatialIndex</span>(integer <span class="param">index_type</span>, text <span class="param">index_name</span>, text <span class="param">index_directory</span>, integer <span class="param">src_id</span>, integer <span class="param">bc_id</span>, integer <span class="param">sc_id</span>, integer <span class="param">buf_id</span>);

Bool <span class="function">FT_CreateEmptySpatialIndex</span>(integer <span class="param">index_type</span>, text <span class="param">apath</span>, integer <span class="param">src_id</span>, integer <span class="param">bc_id</span>, integer <span class="param">sc_id</span>, integer <span class="param">buf_id</span>);

## Description

==FT_CreateEmptySpatialIndex== initializes the structure of a spatial index. The created spatial index is empty; thus, it does not contain any indexed spatial object yet. It returns `true` if the operation is successfully performed and `false` otherwise.

!!! note
	==FT_CreateEmptySpatialIndex== does not insert any spatial object into the created spatial index. ==FT_CreateEmptySpatialIndex== is employed by the workload [FT_CreateSpatialIndex](../../workloads/ft_createspatialindex), which indexes all spatial objects stored in a given spatial dataset by inserting one spatial object by time into the spatial index.

A spatial index consists of some configurations that specify its basic and specific parameters, as well as the spatial dataset that is linked to the spatial index. To build a spatial index, primary key values of relational tables of the [FESTIval's data schema](../../data_schema/overview) should be employed. ==FT_CreateEmptySpatialIndex== has two versions and its parameters are:

* <span class="param">index_type</span> is the type of the spatial index. Integer values from `1` to `10` to respectively represent the `R-tree`, the `R*-tree`, the `Hilbert R-tree`, the `FAST R-tree`, the `FAST R*-tree`, the `FAST Hilbert R-tree`, the `FOR-tree`, the `eFIND R-tree`, the `eFIND R*-tree`, and the `eFIND Hilbert R-tree`. More information about these supported indices are given [here](../../data_schema/overview).
* <span class="param">index_name</span> is the name of the index file.
* <span class="param">index_directory</span> is the directory path that stores the index file.
* <span class="param">apath</span> is the absolute path of the index file.
* <span class="param">src_id</span> is the primary key value of the table `Source`. It indicates the underlying spatial dataset of the spatial index. The spatial dataset determines the origin of spatial objects to be manipulated by the spatial index. For instance, the processing of spatial queries may access the spatial dataset (see [FT_QuerySpatialIndex](../ft_queryspatialindex)).
* <span class="param">bc_id</span> is the primary key value of the table `BasicConfiguration`. It indicates the general parameters of the spatial index, such as the page (node) size in bytes.
* <span class="param">sc_id</span> is the primary key value of the table `SpecializedConfiguration`. It indicates the specific parameters of the spatial index. That is, specific parameters that is only valid for one type of index. For instance, the reinsertion percentage of an R*-tree.
* <span class="param">buf_id</span> if the primary key value of the table `BufferConfiguration`. It indicates the type of the general-purpose buffer manager to be employed by the spatial index. Examples of buffers are LRU and S2Q. If the buffer size is equal to `0`, the spatial index will not employ a general-purpose buffer manager. 

!!! note
	* <span class="param">index_name</span>, <span class="param">index_directory</span>, and <span class="param">apath</span> should not contain special characters.
	* FESTIval provides a set of default values for the aforementioned tables. We recommend that you see the [FESTIval's data schema](../../data_schema/overview) in order to understand the types of configurations that FESTIval manages.

!!! danger "Caution"
	 The connected user of the database must be permission to read and write in the directory storing the index file. Otherwise, an error is returned.


## Examples

``` SQL
--creating an empty R-tree with linear splitting algorithm and other parameters.
SELECT FT_CreateEmptySpatialIndex(1, '/opt/linear_rtree', 7, 6, 18, 4);

--creating an empty eFIND R-tree. The buf_id=1 indicates that this index has not a general-purpose buffer manager because eFIND has its own buffer manager.
SELECT FT_CreateEmptySpatialIndex(8, '/opt/efind_rtree', 5, 53, 50031, 1);
```

## See Also

* After constructing an empty spatial index, we are able to index spatial objects - [FT_Insert](../ft_insert)
* Creating workload - [see the FT_CreateEmptySpatialIndex being used in a workload](../../workloads/ft_createspatialindex)