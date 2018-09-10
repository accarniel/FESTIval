# FESTIval

FESTIval is a framework, implemented as a PostgreSQL extension, for conducting experimental evaluations of spatial indices. The complete documentation of FESTIval is available [here](https://github.com/accarniel/FESTIval/wiki). We are currently working on to make FESTIval publicly available. Feel free to see how FESTIval is useful by accessing the [publications employing FESTIval](https://github.com/accarniel/FESTIval/wiki/Publications).

## Overview

FESTIval (short for *Framework to Evaluate SpaTial Indices*) is a framework, implemented as a PostgreSQL extension, that has the following goal: to provide a common environment that aids the definition, execution, and analyses of experimental evaluations of different spatial indices, which 

* aim to accelerate the spatial query processing in spatial database systems, 
* can assume distinct configurations (i.e., parameter values) and,
* might be designed for specific storage devices (e.g., magnetic disks and flash memories).

PostgreSQL has free license and it is an extensible DBMS. By using its internal library, we are able to news functionalities by using a low level program language (e.g., C language) or high level program language (e.g., pl/pgSQL). More details about its extensibility are given [here](https://www.postgresql.org/docs/current/static/sql-createextension.html).

FESTIval is implemented in C using the extensibility provided by the PostgreSQL's internal library. FESTIval also employs [PostGIS](https://postgis.net/), a PostgreSQL extension that handles spatial objects in relational tables. Therefore, FESTIval acesses spatial objects stored in a column of a relational table (managed by PostGIS) in order to be manipulated by a spatial index. 

## Characteristics

Currently, FESTIval allows us to:

* evaluate disk-based spatial indices (i.e., indices for magnetic disks), such as the [R-tree](https://dl.acm.org/citation.cfm?id=602266).
* evaluate flash-aware spatial indices (i.e, indices for flash memories), such as the [FOR-tree](https://www.sciencedirect.com/science/article/pii/S0957417415000263), [FAST-based indices](https://link.springer.com/article/10.1007/s10707-012-0164-9), and [eFIND-based indices](https://link.springer.com/chapter/10.1007/978-3-319-66917-5_16).
* specify and execute user-defined workloads by using a unique environment.
* access an integrated data schema that stores index configurations and statistical data of executed workloads.
* use any spatial dataset in experiments.
* reproduce executed workloads.

FESTIval manages a schema in the PostgreSQL called *fds*. This schema is automatically created by the FESTIval when the CREATE EXTENSION command is executed in a database. By using this schema, it is possible to collect and compare statistical data of different indices and analysis their results.

## How to Install and Use FESTIval?

Please, access our detailed documentation [here](https://github.com/accarniel/FESTIval/wiki), which provides the step-by-step to perform the installation of FESTIval. It also includes several examples of examples together with a full description of the SQL operations of FESTIval.

### Acknowledgments

This project is supported by grant #2015/26687-8, São Paulo Research Foundation (FAPESP) in Brazil. FESTIval is mantained and developed by Anderson Chaves Carniel. Supervisor: Prof. Dr. Cristina Dutra de Aguiar Ciferri. Collaborator: Prof. Dr. Ricardo Rodrigues Ciferri.

Research conducted at the University of São Paulo in Brazil

