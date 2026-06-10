create table distr1 as shard1 engine Distributed (test_cluster_two_shards_localhost, currentDatabase(), shard1, cityHash64(id))
