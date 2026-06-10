CREATE TABLE x_dist as x ENGINE = Distributed('test_cluster_two_shards', currentDatabase(), x)
