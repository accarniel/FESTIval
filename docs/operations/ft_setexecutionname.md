# FT_SetExecutionName

## Summary

==FT_SetExecutionName== sets the name used to identify collected statistical data. It returns `true` if the setting is successfully performed and `false` otherwise.

## Signature

Bool <span class="function">FT_SetExecutionName</span>(text <span class="param">execution_name</span>, integer <span class="param">loc_stat_data=1</span>);

## Description

==FT_SetExecutionName== sets the name used to identify collected statistical data. It returns `true` if the setting is successfully performed and `false` otherwise. Its parameters are:

* <span class="param">execution_name</span> is the name employed to identifiy collected statistical data of index operations.
* <span class="param">loc_stat_data</span> defines where the statistical data is beging stored. If its value is equal to ``1``, its default value, the statistical data is stored directly in the FESTIval’s data schema. Thus, ==FT_SetExecutionName== updates the default value of the attribute `execution_name` of the relational table `Execution` accordingly. If its value is equal to ``2``, the statistical data is stored in a SQL file that can be latter loaded into the FESTIval’s data schema. Thus, the execution name is only setted during the connection in the database.

After calling this function, the new execution name is adopted in all next performed operations. Therefore, this is useful for give names in workloads.

!!! note
	==FT_SetExecutionName== does not permantely changes the execution name when loc_stat_data is equal to `2` because statitiscal data is stored in a file instead of the FESTIval's data schema.

## Examples

``` SQL
--setting the execution name since we will now insert a new object
SELECT FT_SetExecutionName('a single insertion');

SELECT FT_AInsert('r-tree', '/opt/festival_indices/', 50, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857)
	);

--changing the execution name to adequately represent the purpose of the next operation
SELECT FT_SetExecutionName('a single update');

SELECT FT_AUpdate('/opt/festival_indices/rstartree-brazil_points2017', 50, 
	ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857),
	50, ST_Buffer(ST_GeomFromText('POLYGON((-6349160.26886151 -751965.038197354,-6349160.26886151 -606557.85245731,-6211936.96741955 -606557.85245731,-6211936.96741955 -751965.038197354,-6349160.26886151 -751965.038197354))', 3857), 10)
	);
```

## See Also

* Creating workload - [see the FT_SetExecutionName being used in a workload](../../workloads/ft_createspatialindex)
