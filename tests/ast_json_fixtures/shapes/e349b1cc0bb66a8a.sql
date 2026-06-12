CREATE TABLE dist_00612 AS data_00612 ENGINE = Distributed(test_shard_localhost, currentDatabase(), data_00612, rand())
