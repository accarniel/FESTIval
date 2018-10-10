# An Overview of the FESTIval's data schema - <span class="function">*fds*</span>

The FESTIval's data schema stores information of spatial datasets that can be used by spatial indices, general and specific parameters employed by spatial indices, and statistical data of executed workloads. Collected statistical data can be used in mathematical models to measure the performance of spatial indices, considering employed parameter values, characteristics of the spatial dataset, and the employed storage device.

The figure below provides the logical view of the FESTIval's data schema, called <span class="function">*fds*</span>. This figure only shows the relationships of the relational tables by showing their primary and foreign keys. Primary keys are highlighted. The relationship between two tables are given by a directed row linking the primary key of a table to the foreign key of another table. 

![The logical view of the FESTIval's data schema](../../data_schema.png "The logical view of the FESTIval's data schema")

There are two categories of data managed by FESTIval: (i) [configuration of a spatial index](../overview/#configuration_of_a_spatial_index), and (ii) [statistical data of executed workloads](../overview/#statistical_data_of_executed_workloads). 

## Configuration of a Spatial Index

The configuration of a spatial index consists of four components. Each component is shortly described below and is represented by a relational table (see the specification of a relational table by clicking on its corresponding link):

1. [Source](../spec/#source) - this table stores data about the spatial datasets that can be used by spatial indices.
2. [BasicConfiguration](../spec/#basicconfiguration) - this table stores the general parameters that can be employed by any spatial index. It may includes the specific information regarding the employed storage device as follows:
	* [StorageSystem](../spec/#storagesystem) - this table describes the storage system that the spatial index should employ. This can be further specialized if the storage system is the Flash-DBSim:
		* [FlashDBSimConfiguration](../spec/#flashdbsimconfiguration) - this table stores specific parameters of the flash memory emulated by the Flash-DBSim:
			* [VirtualFlashDevice](../spec/#virtualflashdevice) - parameters of the flash device of the Flash-DBSim.
			* [FlashTranslationLayer](../spec/#flashtranslationlayer) - parameters of the flash translation layer of the Flash-DBSim.
3. [SpecializedConfiguration](../spec/#specializedconfiguration) - this table generalizes the specific parameters of a spatial index. Thus, each specialization of this table represents the specific parameters of a given spatial index. The specialized tables are:
	* [RTreeConfiguration](../spec/#rtreeconfiguration), which refers to the parameters of the R-tree. 
	* [RStartConfiguration](../spec/#rstartreeconfiguration), which refers to the parameters of the R*-tree.
	* [HilbertRTreeConfiguration](../spec/#hilbertrtreeconfiguration), which refers to the parameters of the Hilbert R-tree.
	* [FORTreeConfiguration](../spec/#fortreeconfiguration), which refers to the parameters of the FOR-tree.
	* [FASTConfiguration](../spec/#fastconfiguration), which refers to the parameters of FAST.
	* [eFINDConfiguration](../spec/#efindconfiguration), which refers to the parameters of eFIND.
	
	Further, these table may use the table [OccupancyRate](../spec/#occupancyrate), which stores the minimum and maximum occupancy of nodes.

4. [BufferConfiguration](../spec/#bufferconfiguration) - this table stores the parameters of the buffer manager of the spatial index, if any.

FESTIval provides a script named *[festival-inserts.sql](https://github.com/accarniel/FESTIval/)* that provides an extensive set of tuples for these relational tables. The default spatial indices included by this script can be downloaded at [the wiki page of the FESTIval's Github project](https://github.com/accarniel/FESTIval/wiki/). Users are also encouraged to insert new tuples in such tables in order to create new configurations, as needed. 

In addition, FESTIval internally manages the following tables that represent a spatial index:

* [IndexConfiguration](../spec/#indexconfiguration) - the combination of the primary key values of the four components creates an configuration for a spatial index to be hanled by FESTIval.
* [SpatialIndex](../spec/#spatialindex) - this table represents the spatial index effectively created by the user. Note that different spatial indices can employ the same configuration; however, they can have different contents because of the operations executed in workloads.

!!! note
	Users should not insert new tuples into the tables ==IndexConfiguration== and ==SpatialIndex== because they are internally managed by FESTIval only.


## Statistical Data of Executed Workloads

There are two types of statistical data managed by FESTIval: (i) general statistical data that every index operation can generate, and (ii) statistical data related to the index structure. The relational tables storing statistical data are (see the specification of a relational table by clicking on its corresponding link):

* [Execution](../spec/#execution) - this table stores general statistical data related to the execution of index operations like insertions, updates, and deletions. 
* [ReadWriteOrder](../spec/#spatialindex) - this table stores the order of reads and writes performed on the storage device by the index operations.
* [FlashSimulatorStatistics](../spec/#indexconfiguration) - this table stores statistical data from the flash simulator, if it was used by the spatial index.
* [IndexSnapshot](../spec/#spatialindex) - this table stores statistical data related to the structure of the index.
* [PrintIndex](../spec/#printindex) - this table stores all index pages of the spatial index, allowing users to visualize the structure of the index using a geographical information system like [QGIS](https://qgis.org/).

!!! note
	We do not encourage users to change the structure of the tables storing statistical data, as well as we do not encourage to manually insert new values in such tables. Be careful if you really need to modify something (e.g., consider to make a copy of this content in another temporary table).