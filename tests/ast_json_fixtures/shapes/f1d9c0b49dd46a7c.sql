CREATE TABLE distributed_00588 AS mergetree_00588 ENGINE = Distributed(test_shard_localhost, currentDatabase(), mergetree_00588)
