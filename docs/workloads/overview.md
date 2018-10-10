# Creating and Execution Workloads

FESTIval provides a set of SQL functions that allows users to create and execute workloads by using a common design. By using them, users are able to write SQL functions using the *SQL Procedural Language* of the PostgreSQL (PL/pgSQL). Hence, users create workloads as user-defined functions in PL/pgSQL and execute them in SQL SELECT statements.

Here, we provide a quick guide to help users to manage their workloads. This guide can be seem as tips and best practices for using FESTIval.

## Setting a name for executions

Before executing a user-defined function that executes a set of operations implemented by FESTIval, it is important to provide a name for the execution. For instance:

``` SQL
-- it is a good practice to provide an execution name before executing a simple operation or a set of operations
SELECT FT_SetExecutionName('SPECIFY A MEANINGFUL NAME FOR THE NEXT COMMANDS TO BE EXECUTED');

--now you execute your workload here.
```

## Extracting statistical results

After executing a workload, users can issue SQL SELECT statements to retrieve performance results. For instance, the following command returns the required index time to process a given workload:

``` SQL
SELECT index_time
FROM fds.execution
WHERE execution_name = 'THE MEANINGFUL NAME THAT YOU DEFINED DURING THE EXECUTION OF YOUR WORKLOAD';
```

Note the importance of setting a name for the execution. 

SQL SELECT statements could also include other columns containing statistical values, such as the number of writes and reads. If the workload is executed multiple times, we can write SQL SELECT statements that yield the average index time and its standard deviation. For instance:

``` SQL
SELECT avg(index_time), stddev(index_time)
FROM fds.execution
WHERE execution_name = 'THE MEANINGFUL NAME THAT YOU DEFINED DURING THE EXECUTION OF YOUR WORKLOAD';
```

The aforementioned examples are very simple SQL SELECT statements. Complex queries can also be written, involving joins with other tables of the [FESTIval's data schema](../../data_schema/overview). 

## Examples of Workload

Here, we provide an example of workload that is already included in the FESTIval: the construction of a spatial index inserting one spatial object by time. This workload is called [FT_CreateSpatialIndex](../../workloads/ft_createspatialindex).

You can include your workload here (with the reference of a paper that has employed it, if any) by making Pull Requests on [FESTIval GitHub Repo](https://github.com/accarniel/FESTIval). Alternately, contact Anderson Carniel by sending an email to <accarniel@gmail.com>.