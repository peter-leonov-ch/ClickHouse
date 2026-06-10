CREATE TABLE distributed_01099_c AS local_01099_c ENGINE = Distributed('test_shard_localhost', currentDatabase(), local_01099_c, rand())
