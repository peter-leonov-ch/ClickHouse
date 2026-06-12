CREATE TABLE test_distributed_2 AS test_local_2 ENGINE = Distributed('test_shard_localhost', currentDatabase(), test_local_2, rand())
