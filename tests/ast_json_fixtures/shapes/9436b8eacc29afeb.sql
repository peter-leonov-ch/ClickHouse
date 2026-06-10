CREATE TABLE distributed_01099_a AS local_01099_a ENGINE = Distributed('test_shard_localhost', currentDatabase(), local_01099_a, rand())
