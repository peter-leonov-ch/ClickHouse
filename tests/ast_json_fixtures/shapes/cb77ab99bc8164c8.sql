create table dist_01270 as data_01270 Engine=Distributed(test_cluster_two_shards, currentDatabase(), data_01270, key)
