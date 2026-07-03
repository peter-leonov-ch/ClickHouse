CREATE TABLE t02006 on cluster test_shard_localhost (d Date) 
ENGINE = MergeTree ORDER BY d
format Null
