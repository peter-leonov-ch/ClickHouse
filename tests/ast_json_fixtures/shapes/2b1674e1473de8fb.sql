create table table_dist engine = Distributed('test_cluster_two_shards', currentDatabase(),table_local) AS table_local
