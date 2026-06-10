CREATE TABLE y_dist as y ENGINE = Distributed('test_cluster_two_shards_localhost', currentDatabase(), y)
