CREATE TABLE t_03363_hive_only_partition_columns (country String, year UInt16) ENGINE = S3(s3_conn, partition_strategy='hive') PARTITION BY (year, country)
