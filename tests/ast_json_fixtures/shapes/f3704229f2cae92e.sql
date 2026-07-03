CREATE TABLE t1_local ON CLUSTER test_shard_localhost(partition_col_1 String, tc1 int,tc2 int)ENGINE=MergeTree() PARTITION BY partition_col_1 ORDER BY tc1
