create table distr2 as shard2 engine Distributed (test_cluster_two_shards_localhost, currentDatabase(), shard2, cityHash64(id))
