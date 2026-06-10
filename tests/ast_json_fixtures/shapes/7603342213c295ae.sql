CREATE TABLE distributed_table2 AS source_table2
ENGINE = Distributed('test_shard_localhost', currentDatabase(), source_table2)
