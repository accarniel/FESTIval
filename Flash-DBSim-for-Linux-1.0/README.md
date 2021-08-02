# To cite Flash-DBSim for Linux

Please mention the following article to cite the Flash-DBSim for Linux:

* [Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. FESTIval: A versatile framework for conducting experimental evaluations of spatial indices. MethodsX, vol. 7, 2020, 100695, 2020.](https://www.sciencedirect.com/science/article/pii/S2215016119302717)

Please mention the applicability of the Flash-DBSim for Linux by citing one of the following papers:

* [Carniel, A. C.; Silva, T. B.; Bonicenha, K. L. S.; Ciferri, R. R.; Ciferri, C. D. A. Analyzing the Performance of Spatial Indices on Flash Memories using a Flash Simulator. In Proceedings of the 32nd Brazilian Symposium on Databases (SBBD), p. 40-51, 2017.](http://www.lbd.dcc.ufmg.br/colecoes/sbbd/2017/003.pdf)
* [Carniel, A. C.; Silva, T. B.; Ciferri, C. D. A. Understanding the Applicability of Flash Simulators on the Experimental Evaluation of Spatial Indices. In 9th Annual Non-volatile Memories Workshop (NVMW), p. 1-2, 2018.](https://www.researchgate.net/publication/327424917_Understanding_the_Applicability_of_Flash_Simulators_on_the_Experimental_Evaluation_of_Spatial_Indices)

# Overview

Flash-DBSim for Linux ports the flash simulator [Flash-DBSim](http://kdelab.ustc.edu.cn/flash-dbsim/index_en.html), originally implemented for Windows, to be used in Linux based systems.
A flash simulator emulates in the memory the behavior of a flash memory. Thus, it consists of an interesting and promising environment to execute experiments, such as the evaluation of index structures using [FESTIval](https://accarniel.github.io/FESTIval/).

Flash-DBSim for Linux also provides a C-API.

# Compilation and Installation

To compile Flash-DBSim for Linux first execute the following command in the terminal:

```
make all
```

Then, Flash-DBSim will be compiled as a shared library object with its C-API. To compile and execute the running example, which uses the C-API, use the following commands:

```
cd Example
make
make run
```

# Related Project

Flash-SBSim is a dependency of [FESTIval](https://accarniel.github.io/FESTIval/). It is used by FESTIval if the storage device storing a spatial index is simulating the behavior of a flash memory.

# Developers

* [Anderson Chaves Carniel](https://accarniel.github.io/)
* [Tamires Brito da Silva](https://github.com/tamiresbrito)
