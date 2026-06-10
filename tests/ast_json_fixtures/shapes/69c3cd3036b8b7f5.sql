create table dist_01320 as data_01320 Engine=Distributed(test_cluster_two_shards, currentDatabase(), data_01320, key + rand())
