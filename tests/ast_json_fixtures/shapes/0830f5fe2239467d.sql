create table t2_all as t2_local engine Distributed(test_cluster_two_shards_localhost, test_02115, t2_local, rand())
