CREATE TABLE test_s64_distributed AS test_s64_local ENGINE = Distributed('test_shard_localhost', currentDatabase(), test_s64_local, rand())
