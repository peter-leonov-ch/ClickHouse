CREATE TABLE distributed_table1 AS source_table1
ENGINE = Distributed('test_shard_localhost', currentDatabase(), source_table1)
