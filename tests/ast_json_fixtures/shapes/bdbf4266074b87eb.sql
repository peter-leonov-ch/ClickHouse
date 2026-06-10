CREATE TABLE distributed_00609 AS mergetree_00609 ENGINE = Distributed(test_shard_localhost, currentDatabase(), mergetree_00609)
