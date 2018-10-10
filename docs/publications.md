The first version of FESTIval was published in:

* [Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. Experimental Evaluation of Spatial Indices with FESTIval. In Proceedings of the Satellite Events of the 31st Brazilian Symposium on Databases (SBBD), p. 123–128, 2016.](https://www.researchgate.net/publication/310295040_Experimental_Evaluation_of_Spatial_Indices_with_FESTIval)

> This version only encompasses the implementation of the following disk-based spatial indices: the R-tree and the R*-tree.

FESTIval has been used as the main tool to **understand the impact of SSDs on the spatial indexing context**, as described in:

* [Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. The Performance Relation of Spatial Indexing on Hard Disk Drives and Solid State Drives. In Proceedings of the Brazilian Symposium on GeoInformatics, p. 263-174, 2016.](https://www.researchgate.net/publication/311258275_The_Performance_Relation_of_Spatial_Indexing_on_Hard_Disk_Drives_and_Solid_State_Drives)

* [Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. Analyzing the Performance of Spatial Indices on Hard Disk Drives and Flash-based Solid State Drives. Journal of Information and Data Management 8 (1), p. 34-49, 2017.](https://seer.ufmg.br/index.php/jidm/article/view/4579)

FESTIval has also been employed to **measure the performance of gains of eFIND**, a generic and efficient framework that transforms a disk-based spatial index into a flash-aware spatial index (e.g., transforms an R-tree to an eFIND R-tree). eFIND is described in:

* [Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. A Generic and Efficient Framework for Spatial Indexing on Flash-based Solid State Drives. In Proceedings of the 21st European Conference on Advances in Databases and Information Systems (ADBIS), p. 229-243, 2017.](https://link.springer.com/chapter/10.1007/978-3-319-66917-5_16)

FESTIval also provides support for **flash simulators** (e.g., [FlashDBSim](http://kdelab.ustc.edu.cn/flash-dbsim/index_en.html)) when analyzing the performance of spatial indices, such as reported in:

* [Carniel, A. C.; Silva, T. B.; Bonicenha, K. L. S.; Ciferri, R. R.; Ciferri, C. D. A. Analyzing the Performance of Spatial Indices on Flash Memories using a Flash Simulator. In Proceedings of the 32nd Brazilian Symposium on Databases (SBBD), p. 40-51, 2017.](http://www.lbd.dcc.ufmg.br/colecoes/sbbd/2017/003.pdf)
* [Carniel, A. C.; Silva, T. B.; Ciferri, C. D. A. Understanding the Applicability of Flash Simulators on the Experimental Evaluation of Spatial Indices. In 9th Annual Non-volatile Memories Workshop (NVMW), p. 1-2, 2018.](https://www.researchgate.net/publication/327424917_Understanding_the_Applicability_of_Flash_Simulators_on_the_Experimental_Evaluation_of_Spatial_Indices)


In order to facilitate the execution of experiments, FESTIval provides [**default spatial datasets**](https://github.com/accarniel/FESTIval/wiki/Default-Datasets). They are specified in:

* [Carniel, A. C.; Ciferri, R. R.; Ciferri, C. D. A. Spatial Datasets for Conducting Experimental Evaluations of Spatial Indices. In Proceedings of the 32nd Brazilian Symposium on Databases (SBBD) - Workshop Dataset Showcase, p. 286-295, 2017.](https://www.researchgate.net/publication/320270968_Spatial_Datasets_for_Conducting_Experimental_Evaluations_of_Spatial_Indices)

Originally, FESTIval has been developed in the context of a **Ph.D. work**. The main goals of this Ph.D. work are described in:

* [Carniel, A. C.; Ciferri, C. D. A. Spatial Indexing in Flash Memories: Proposal of an Efficient and Robust Spatial Index with Durability. In Proceedings of the Satellite Events of the 31st Brazilian Symposium on Databases (SBBD), p. 66-73, 2016.](http://sbbd2016.fpc.ufba.br/sbbd2016/wtdbd/wtdbd-2016_paper_8.pdf)
* [Carniel, A. C. Spatial Indexing on Flash-based Solid State Drives. In Proceedings of the VLDB 2018 PhD Workshop, p. 1-4, 2018.](http://ceur-ws.org/Vol-2175/paper09.pdf)