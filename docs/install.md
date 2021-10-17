# Installation Guide

The following guide will help you to install FESTIval from the source onto various operating systems based on Linux.

## Downloading FESTIval

FESTIval is publicly available at GitHub and you can download its most recent version at [GitHub page](https://github.com/accarniel/FESTIval/releases).

## Dependencies

The dependencies of FESTIval are:

* [PostgreSQL](https://www.postgresql.org/) 9.5 (or later) 
* [PostGIS](https://postgis.net/) 2.2.1 (or later until 2.5 - this means that FESTIval **is not** compatible with PostGIS >= 3) 
* [GEOS library](https://trac.osgeo.org/geos) (the version compatible with the PostGIS version)

!!! note
	Dependencies .so of PostGIS should be located at the directory named `lib` of PostgreSQL (e.g., PostgreSQL/9.5/lib)

## Installing

In the terminal at the directory of the source code of FESTIval:

```
make postgis=/PATH/TO/YOUR/POSTGIS_SOURCE_CODE
sudo make install
```

!!! note
	You have to inform the full path of the PostGIS's source code in the parameter <span class="param">postgis</span> (i.e., the root directory of the PostGIS). This means that you need to install the PostGIS from its source code.

## Enabling FESTIval in a Database

Connect to your database using *pgAdmin* or *psql*, and execute the following SQL statements to enable FESTIval.

``` SQL
CREATE EXTENSION festival;
```

!!! note
	The database need to be created with PostGIS template or has the postgis extension enabled (i.e., by executing the SQL statement `CREATE EXTENSION postgis;`).

!!! note "Backups"
	If you need to do a backup of the FESTIval's data schema, please copy the content of all tables from **fds** to another schema and then make the backup on this new schema. This procedure is needed because of the management of extension of PostgreSQL.

This command will create several tables in a new schema called [**fds**](../data_schema/overview). By using the tables of this schema, you are able to retrieve statistical data after executing operations handling spatial indices.

To disable FESTIval in a database, you can simply execute the following SQL statement:

``` SQL
DROP EXTENSION festival;
```

!!! danger
	Be careful before dropping a table. Disabling FESTIval will also delete its data schema, including their stored data!

FESTIval also provides a set of files that contain default values for relational tables of **fds**. It is available in the directory called **default-values**. After extracting the file *default-values-files.tar.xz*, you will be able to execute the *festival-inserts.sql* as follows. 

```
psql -U user -d database -vfestivaldir=/f_path/ -f /f_path/festival-inserts.sql
```

where <span class="param">user</span> is the user name of the PostgreSQL, <span class="param">database</span> is the database name in which FESTIval is enabled, and <span class="param">/f_path/</span> is the full path of the FESTIval’s source code.

