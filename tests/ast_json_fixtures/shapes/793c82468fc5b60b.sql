CREATE TABLE IF NOT EXISTS dist_01213 AS local_01213 ENGINE = Distributed(test_cluster_two_shards_localhost, currentDatabase(), local_01213, id)
