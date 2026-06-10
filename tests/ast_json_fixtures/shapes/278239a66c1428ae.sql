create table t1_distr as t1_shard engine Distributed(test_cluster_two_shards_localhost, test_01103, t1_shard, id)
