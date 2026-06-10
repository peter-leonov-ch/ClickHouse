CREATE TABLE test_distributed_1 AS test_local_1 ENGINE = Distributed('test_shard_localhost', currentDatabase(), test_local_1, rand())
