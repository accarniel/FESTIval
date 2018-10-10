# Specification of <span class="function">*fds*</span>

The relational tables composing <span class="function">*fds*</span> are described as follows.

## Source

This table stores needed information of the spatial dataset that can be indexed. Here, a spatial dataset is represented by a relational table of the PostgreSQL containing PostGIS objects. FESTIval needs to know details from this relational table such as its name, the name of its schema, the name of the column storing spatial objects, and the column that corresponds to the primary key. 

!!! note
	When processing spatial queries using a spatial index, the information from the table Source is useful to process the refinement step. Hence, it is important to keep the correspondence between the spatial dataset and its spatial index.

FESTIval offers some default datasets that can be indexed. They are available [here](https://github.com/accarniel/FESTIval/wiki/).

The columns of ==Source== are described as follows:

| Column        | Description           | 
| ------------- |:-------------|
| *src_id*      | It is the primary key and is an auto-increment field. | 
| *schema_name* | It stores the name of the schema that contains the spatial objects.      | 
| *table_name*  | It stores the name of the table that contains the spatial objects.     | 
| *column_name* | It stores the name of the column that contains the spatial objects. |
| *pk_name*     | It stores the name of the primary key column of the table that contains the spatial objects. |

## BasicConfiguration

This table stores the general parameters that can be employed by any spatial indices. In addition, the type of storage system (table ==StorageSystem==) is also specified. 

!!! note
	Every spatial index is based on a tuple of this table.

The columns of ==BasicConfiguration== are described as follows:

| Column        | Description           | 
| ------------- |:-------------|
| *bc_id*      | It is the primary key and is an auto-increment field. | 
| *ss_id* | It is the foreign key that points to the table ==StorageSystem==.      | 
| *page_size*  | It stores the index page size in bytes to be used by the spatial index. This value must be power of 2.     | 
| *io_access* | It specifies the type of I/O access, which can be the conventional method (`'NORMAL ACCESS'` value) and the DIRECT I/O method (`'DIRECT ACCESS'` value). The conventional method employs the library `libio.h`, while the DIRECT I/O method employs the library `fcntl.h`. |
| *refinement_type*     | It specifies the algorithms to be employed by the refinement step when processing spatial queries. It can employ the GEOS library (`'ONLY GEOS'` value) and the GEOS library together with the PostGIS point polygon check algorithm (`'GEOS AND POINT POLYGON CHECK FROM POSTGIS'` value). |


## StorageSystem

This table stores the general description of storage devices that can be linked by the table ==BasicConfiguration==. It can also be specialized to store the parameters of an emulated flash memory (see table ==FlashDBSimConfiguration==).

!!! note
	Every spatial index is, therefore, manipulated in a storage system.

The columns of ==StorageSystem== are described as follows:

| Column        | Description           | 
| ------------- |:-------------|
| *ss_id*      | It is the primary key and can be used in other specialized tables (e.g., ==FlashDBSimConfiguration==). | 
| *storage_system* | It is the type of the storage system, which can be a magnetic disk (`'HDD's` value), flash-based solid state drive (`'Flash SSD'`), or an emulated flash memory (`'FlashDBSim'` value). If the storage system is an emulated flash memory system, more parameters are obtained from the FESTIval's data schema in order to simulate the flash memory.      | 
| *description*  | It stores the description regarding the storage device employed in the experiment, such as its full specification and/or datasheet.     | 

## FlashDBSimConfiguration

This table stores the needed parameters to emulate a flash memory using the [Flash-DBSim](https://ieeexplore.ieee.org/document/5234967), which is available [here](http://kdelab.ustc.edu.cn/flash-dbsim/index_en.html). We have ported the Flash-DBSim to linux-based systems in a project called [**Flash-DBSim for Linux**](https://github.com/accarniel/Flash-DBSim-for-Linux); thus, **Flash-DBSim for Linux** is a dependency of FESTIval.

!!! note
	Every emulated flash memory in FESTIval currently employs the Flash-DBSim.

The columns of ==FlashDBSimConfiguration== are described as follows:

| Column        | Description           | 
| ------------- |:-------------|
| *ss_id*      | It is part of the primary key and is a inherited value from the primary key of ==StorageSystem==. | 
| *ftl_id* | It is part of the primary key and points to the parameters related to the Flash Translation Layer of the emulated flash memory.      | 
| *vfd_id*  | It is part of the primary key and points to the parameters related to the Virtual Flash Device of the emulated flash memory.     | 

!!! note
	Every tuple of ==FlashDBSimConfiguration== is a combination of tuples from the following tables: ==Virtual Flash Device== and ==FlashTranslationLayer==. Hence, ==FlashDBSimConfiguration== is a weak entity.

## VirtualFlashDevice

This table stores the parameters of the Virtual Flash Device of an emulated flash memory using Flash-DBSim.

The columns of ==VirtualFlashDevice== are described as follows:

| Column        | Description           | 
| ------------- |:-------------|
| *vfd_id*      | It is the primary key and is an auto-increment field. | 
| *nand_device_type* | It is an integer value that identifies the type of the virtual device. Although the Flash-DBSim has four possible virtual devices, FESTIval only allows two types of devices because of the types of statistical that can be collected. The accepted values are: `3` and `4`. While the type `3` manages the emulated flash memory in the main memory, the type `4` manages the emulated flash memory in an external file handled by the Flash-DBSim. | 
| *block_count*  | It determines the number of blocks of the emulated flash memory.     | 
| *page_count_per_block*      | It defines the number of flash pages per block. | 
| *page_size1*      | It defines the flash page size in bytes. This size is used to store data. | 
| *page_size2*      | It defines the size in bytes of spare area per flash page. | 
| *erase_limitation*      | It specifies the number of erases that a block can handle (i.e., it defines the endurance of the emulated flash memory). | 
| *read_random_time*      | It specifies the required time to perform a random read. | 
| *read_serial_time*      | It specifies the required time to perform a sequential read. | 
| *program_time*      | It specifies the required time to perfom a write operation. | 
| *erase_time*      | It specified the required time to perform an erase operation. | 

!!! note
	Any unit time like seconds and milliseconds employed by some columns of ==VirtualFlashDevice==. Note that the same unit time employed here will be used in the corresponding columns that collect elapsed times in the table ==FlashSimulatorStatistics==.

## FlashTranslationLayer

This table stores the parameters of the Flash Translation Layer of an emulated flash memory using Flash-DBSim.

The columns of ==FlashTranslationLayer== are described as follows:

| Column        | Description           | 
| ------------- |:-------------|
| *ftl_id*      | It is the primary key and is an auto-increment field. | 
| *ftl_type* | It is the type of the Flash Translation Layer. Currently, it can assume only the value `1` because Flash-DBSim supports only one type of Flash Translation Layer.    | 
| *map_list_size*  | It stores the size of the map list between physical addresses of the virtual flash device and the logical addresses used by the flash translation layer. Commonly, it corresponds to the following formula: *block_count* * *page_count_per_block*. | 
| *wear_leveling_threshold* | It specifies the threshold for wear-leveling control (it is related to the improvement of the endurance of the flash memory). |

!!! note 
	The columns used in the formula of the column *map_list_size* are from ==VirtualFlashDevice==; therefore, be careful when creating a ==FlashDBConfiguration== by combining the primary key values of the tables ==VirtualFlashDevice== and ==FlashTranslationLayer==.    

## SpecializedConfiguration

## RTreeConfiguration

## RStartConfiguration

## HilbertRTreeConfiguration

## FORTreeConfiguration

## FASTConfiguration

## eFINDConfiguration

## OccupancyRate

## BufferConfiguration

## IndexConfiguration

## SpatialIndex

## Execution

## ReadWriteOrder

## FlashSimulatorStatistics

## IndexSnapshot

## PrintIndex