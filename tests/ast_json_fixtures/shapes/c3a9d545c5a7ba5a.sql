create table t1_all as t1_local engine Distributed(test_cluster_two_shards_localhost, test_02115, t1_local, rand())
