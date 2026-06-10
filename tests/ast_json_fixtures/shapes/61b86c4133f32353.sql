create table dist_01756_str as data_01756_str engine=Distributed(test_cluster_two_shards, currentDatabase(), data_01756_str, cityHash64(key))
