# An Overview of the FESTIval's Operations

FESTIval provides a set of SQL functions that allows users to create and execute workloads by using a common design. Each SQL function has the prefix `FT` and calls one C function internally implemented in the FESTIval’s internal library that is responsible for performing the desired processing. The main advantage of this strategy is that complex implementations are hidden from users, who now can manage and test indices under a same environment.

There are three types of operations: (i) [general operations](../overview/#general_operations), (ii) [auxiliary operations](../overview/#auxiliary_operations), and (iii) [atomic operations](../overview/#atomic_operations).

## General Operations

These operations are responsible for handling index structures. They are:

* [**FT_CreateEmptySpatialIndex**](../ft_createemptyspatialindex) initializes the structure of a spatial index. The created spatial index is empty; thus, it does not contain any indexed spatial object yet. 
* [**FT_Insert**](../ft_insert) makes the insertion of a spatial object together with its primary key value into a spatial index. 
* [**FT_Delete**](../ft_delete) makes the deletion of a spatial object indexed in a spatial index. 
* [**FT_Update**](../ft_update) makes the update of a spatial object indexed in a spatial index. 
* [**FT_QuerySpatialIndex**](../ft_queryspatialindex) executes a spatial query using a given spatial index. 
* [**FT_ApplyAllModificationsForFAI**](../ft_applyallmodificationsforfai) applies all the modifications stored in the specialized write buffer of a flash-aware spatial index.
* [**FT_ApplyAllModificationsFromBuffer**](../ft_applyallmodificationsfrombuffer) applies all the modifications stored in the general-purpose in-memory buffer of the index spatial, if any. 

## Auxiliary Operations

These operations are responsible for managing statistical data. They are:

* [**FT_StartCollectStatistics**](../ft_startcollectstatistics) defines the moment in which FESTIval should collect statistical data from executed operations. 
* [**ST_CollectOrderOfReadWrite**](../ft_collectorderofreadwrite) indicates that the order of reads and writes should be also collected. 
* [**FT_StoreStatisticalData**](../ft_storestatisticaldata) stores the collected statistical data. 
* [**FT_StoreIndexSnapshot**](../ft_storeindexsnapshot) collects and stores statistical data related to the index structure. 
* [**FT_SetExecutionName**](../ft_setexecutionname) sets the name used to identify collected statistical data. 


## Atomic Operations

These operations are responsible for handling index structures, **as well as** for collecting and storing related statistical data. Therefore, they are combinations of general and auxiliary operations. The atomic operations are:

* [**FT_AInsert**](../ft_ainsert) is the atomic version of *FT_Insert*. It makes the insertion of a spatial object together with its primary key value in a spatial index, and collects and stores related statistical data. 
* [**FT_ADelete**](../ft_adelete) is the atomic version of *FT_Delete*. It makes the deletion of a spatial object indexed in a spatial index, and collects and stores related statistical data. 
* [**FT_AUpdate**](../ft_aupdate) is the atomic version of *FT_Update*. It makes the update of a spatial object indexed in a spatial index, and collects and stores related statistical data. 
* [**FT_AQuerySpatialIndex**](../ft_aqueryspatialindex) is the atomic version of *FT_QuerySpatialIndex*. It executes a spatial query using a given spatial index, and collects and stores related statistical data. 