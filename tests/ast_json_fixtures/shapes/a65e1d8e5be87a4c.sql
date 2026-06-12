create table dist_02000 as data_02000 Engine=Distributed(test_cluster_two_shards, currentDatabase(), data_02000, key)
