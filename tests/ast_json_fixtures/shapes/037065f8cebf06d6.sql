create table dist_01755 as data_01755 Engine=Distributed(test_cluster_two_shards, currentDatabase(), data_01755, i)
