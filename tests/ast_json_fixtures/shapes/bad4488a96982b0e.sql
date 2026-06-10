CREATE TABLE test_u64_distributed AS test_u64_local ENGINE = Distributed('test_shard_localhost', currentDatabase(), test_u64_local, rand())
