CREATE TABLE foo_distributed AS foo_local
ENGINE = Distributed('test_cluster_two_shards_localhost', currentDatabase(), foo_local)
