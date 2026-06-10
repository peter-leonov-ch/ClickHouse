create table dist_01071 as data_01071 Engine=Distributed(test_cluster_two_shards, currentDatabase(), data_01071)
