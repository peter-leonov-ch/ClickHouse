create table t2_distr as t2_shard engine Distributed(test_cluster_two_shards_localhost, test_01103, t2_shard, id)
