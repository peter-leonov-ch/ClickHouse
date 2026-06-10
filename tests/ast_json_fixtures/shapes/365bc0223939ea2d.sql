CREATE TABLE IF NOT EXISTS test2_d as test2
ENGINE = Distributed(test_cluster_two_shard_three_replicas_localhost, currentDatabase(), test2, id)
