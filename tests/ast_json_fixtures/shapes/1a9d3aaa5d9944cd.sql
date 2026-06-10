CREATE TABLE dist_1 AS mem1 Engine=Distributed(test_shard_localhost, currentDatabase(), mem1)
