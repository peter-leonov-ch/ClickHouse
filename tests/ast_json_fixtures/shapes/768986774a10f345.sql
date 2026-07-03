select max(i) from cluster(test_cluster_two_shards, currentDatabase(), test) where i < 20 FORMAT JSONCompact
