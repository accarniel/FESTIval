--inserting default values
\cd :festivaldir

\echo inserting basic values
\i default-values/first_inserts.sql

\echo inserting specialized configurations
\i default-values/disk-based_spatial_indices_insert_sc.sql
\i default-values/fast-based_spatial_indices_insert_sc.sql
\i default-values/fortree_insert_sc.sql
\i default-values/efind_insert_sc.sql

\echo inserting r-tree, r*-tree, and hilbert r-tree configurations
\i default-values/disk-based_spatial_indices_insert.sql

\echo inserting fast configurations
\i default-values/fast-based_spatial_indices_insert.sql

\echo inserting for-tree configurations
\i default-values/fortree_insert.sql

\echo inserting efind configurations
\i default-values/efind_insert.sql

\echo finishing...
\i default-values/final_sequence.sql
