# Welcome to the FESTIval's Documentation!

[comment]: <> (Check later if the CommonMark is supported by mkdocs.)

<div markdown="1" class="row">

	<div markdown="1" class="column">
		<img src="FESTIval-logo.png" alt="FESTIval logo" title="FESTIval logo" style="padding-right: 10px"/>
	</div>

	<div markdown="1" class="column" style="padding-left: 20px">
		<table>
			<tr>
				<th>Download</th>
				<th>Source code</th>
			</tr>	
			<tr>
				<td><a href="https://github.com/accarniel/FESTIval/archive/v1.0.zip" target="_blank">FESTIval-v1.0</a></td>
				<td><a href="https://github.com/accarniel/FESTIval" target="_blank">Github Project</a></td>
			</tr>
		</table>
	</div>
</div>


To cite FESTIval, please mention its following journal article:

* [Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. FESTIval: A versatile framework for conducting experimental evaluations of spatial indices. MethodsX, vol. 7, 2020, 100695, 2020.](https://www.sciencedirect.com/science/article/pii/S2215016119302717)

This article deeply describes FESTIval and its internal structures.

## Overview

FESTIval (short for *Framework to Evaluate SpaTial Indices*) is a framework with the following goal: to provide a common environment that aids the definition, execution, and analyses of experimental evaluations of spatial indices. Conducting experimental evaluation of spatial indices is a challenging task because they 

* are complex structures that accelerate the spatial query processing in spatial database systems and geographical information systems, 
* can assume distinct configurations (i.e., parameter values) and,
* might be designed for specific storage devices (e.g., magnetic disks and flash memories); thus, a wide range of different spatial indices has been proposed in the literature.

FESTIval is implemented in C as a PostgreSQL extension. [PostgreSQL](https://www.postgresql.org/) is a free, open-source database management system widely employed by the academia and industry. By using its internal library, we are able to add new functionalities by using a low level program language (e.g., C language) or high level program language (e.g., pl/pgSQL). More details about its extensibility are given [here](https://www.postgresql.org/docs/current/static/sql-createextension.html).

FESTIval also employs [PostGIS](https://postgis.net/), a PostgreSQL extension that handles spatial objects in relational tables. Therefore, FESTIval acesses spatial objects stored in a column of a relational table (managed by PostGIS) in order to be manipulated by spatial indices. 

## Features

Currently, FESTIval allows users to:

* evaluate disk-based spatial indices (i.e., indices for magnetic disks): [R-tree](https://dl.acm.org/citation.cfm?id=602266), [R*-tree](https://dl.acm.org/doi/10.1145/93597.98741), and [Hilbert R-tree](http://www.vldb.org/conf/1994/P500.PDF).
* evaluate flash-aware spatial indices (i.e, indices for flash memories): [FOR-tree](https://www.sciencedirect.com/science/article/pii/S0957417415000263), [FAST-based indices](https://link.springer.com/article/10.1007/s10707-012-0164-9), and [eFIND-based indices](https://www.sciencedirect.com/science/article/abs/pii/S0306437918300899).
* specify and execute user-defined workloads by using a unique environment.
* access an integrated data schema that stores index configurations and statistical data of executed workloads.
* use any spatial dataset in experiments. [Default spatial datasets](https://github.com/accarniel/FESTIval/wiki/) are also provided.
* reproduce executed workloads.

FESTIval manages a schema in the PostgreSQL called [*fds*](../data_schema/overview). This schema is automatically created by the FESTIval when the `CREATE EXTENSION` command is executed in a database. By using this schema, it is possible to collect and compare statistical data of different indices and analysis their performance results.

## Applicability 

FESTIval has been employed in many experimental evaluations conducted and reported in research papers, as listed in our [Publications page](publications). In general, FESTIval has been applied to:

* better understand the performance behavior of spatial indices in different storage devices, and
* validate new proposals of spatial indices.

Please make a Pull Request to add your work on the Publications page using FESTIval!
#
### Acknowledgments <!-- TOC omit:true -->

This project was supported by grant #2015/26687-8, São Paulo Research Foundation (FAPESP) in Brazil (2015-2018). FESTIval is mantained and developed by [Anderson Chaves Carniel](https://accarniel.github.io/). 

Research conducted at the University of São Paulo and the Federal University of Technology - Paraná in Brazil.
