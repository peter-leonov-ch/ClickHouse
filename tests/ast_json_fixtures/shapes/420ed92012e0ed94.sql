CREATE TABLE IF NOT EXISTS test_d as test
ENGINE = Distributed(test_cluster_one_shard_three_replicas_localhost, currentDatabase(), test)
