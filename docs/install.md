# Installation Guide

The following guide will help you to install FESTIval from the source onto various operating systems based on Linux.

## Downloading FESTIval

We are currently working on making FESTIval publicly available at its [GitHub page](https://github.com/accarniel/FESTIval).

## Dependencies

The FESTIval dependencies are:

* [PostgreSQL](https://www.postgresql.org/) 9.5 (or later) 
* [PostGIS](https://postgis.net/) 2.2.1 (or later) 
* [GEOS library](https://trac.osgeo.org/geos) (compatible with PostGIS)
* [Flash-DBSim for Linux](https://github.com/accarniel/flash-dbsim-for-linux)

!!! note
	Dependencies .so of PostGIS should be located at the directory named `lib` of PostgreSQL (e.g., PostgreSQL/9.5/lib)

## Configuration

Open the `Makefile` using your favorite editor and modfity the following lines as follows.

First part to be modified:

```
POSTGIS_SOURCE=/opt/postgis-2.2.1
FLASHDBSIM_SOURCE=/opt/flash-dbsim-for-linux-so-c-api
```

Second part to be modified:

```
SHLIB_LINK = $(POSTGIS_SOURCE)/libpgcommon/libpgcommon.a $(POSTGIS_SOURCE)/postgis/postgis-2.2.so -L/usr/local/lib -lgeos_c -llwgeom -lrt

PG_CPPFLAGS = -I/usr/local/include -I$(POSTGIS_SOURCE)/liblwgeom/ -I$(POSTGIS_SOURCE)/libpgcommon/ -I$(POSTGIS_SOURCE)/postgis/ -I/usr/include/ -fPIC

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
```

Where you should check the following paths:

* <span class="param">/opt/flash-dbsim-for-linux-so-c-api</span> is the directory of the source code of the C-API of the *Flash-DBSim for Linux*. *Flash-DBSim for Linux* is a sub-project of FESTIval that aimed at porting [Flash-DBSim](http://kdelab.ustc.edu.cn/flash-dbsim/index_en.html) to Linux-based systems. Its compilation and installation guide is available [here](https://github.com/accarniel/flash-dbsim-for-linux).
* <span class="param">/opt/postgis-2.2</span> is the directory of the source code of PostGIS. The version may vary.
* <span class="param">/usr/local/lib</span> is the directory of the installed libraries of the operating system (including GEOS). It should be modified if really needed.
* <span class="param">/usr/local/include</span> and <span class="param">/usr/include/</span> are directories of needed headers from libraries installed in the operating system. These directories are standards directories of a Linux system and should be modified if really needed.

## Installing

In the terminal at the directory of the source code of FESTIval:

```
sudo make
sudo make install
```

## Enabling FESTIval in a Database

Connect to your database using *pgAdminIII* or *psql*, and execute the following SQL statements to enable FESTIval.

``` SQL
CREATE EXTENSION festival;
```

!!! note
	The database need to be created with PostGIS template or has the postgis extension enabled (i.e., by executing the SQL statement `CREATE EXTENSION postgis;`).


!!! note "Backups"
	If you need to do a backup of the FESTIval's data schema, please copy the content of all tables from **fds** to another schema and then make the backup on this new schema. This procedure is needed because of the management of extension of PostgreSQL.

This command will create several tables in a new schema called [**fds**](data_schema/data_schema_overview). By using the tables of this schema, you are able to retrieve statistical data after executing operations handling spatial indices.

To disable FESTIval in a database, you can simply execute the following SQL statement:

``` SQL
DROP EXTENSION festival;
```

!!! danger
	Be careful before dropping a table. Disabling FESTIval will also delete its data schema, including their stored data!
