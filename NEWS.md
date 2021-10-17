# FESTIval 1.1.1

## Minor changes

* Some improvements in the Makefile.
* Improvements of the compatibility between FESTIval and PostgreSQL/PostGIS.
* Updates in the documentation of the FESTIval to better describe the installation guide.

# FESTIval 1.1.0

## Major changes

* It is compatible with PostgreSQL >= 9.5 and with PostGIS >= 2.2 and <= 2.5 (#2 - @pedrobertella).
* It adds a script to discover the versions of PostgreSQL and PostGIS used in the compilation of the FESTIval (#2 - @pedrobertella).
* The [FlashDBSim-For-Linux's source code](https://github.com/accarniel/Flash-DBSim-for-Linux) is now attached to FESTIval in the directory _libraries_ (#3 - @pedrobertella).
* The Makefile is changed to also compile the FlashDBSim-For-Linux beforing compiling FESTIval (#3 - @pedrobertella).

# FESTIval 1.0.1

## Minor changes

* Small corrections in the memory management of the file `fast_log_module.c`.

# FESTIval 1.0.0

* This is the first release of _FESTIval_. It is compatible with the PostgreSQL 9.5 and PostGIS 2.2. 
* It is fully described in the following research paper: [Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. FESTIval: A versatile framework for conducting experimental evaluations of spatial indices. MethodsX, vol. 7, 2020, 100695, 2020.](https://www.sciencedirect.com/science/article/pii/S2215016119302717)
