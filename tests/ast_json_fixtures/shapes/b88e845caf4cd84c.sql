create table dist2_01071 as data2_01071 Engine=Distributed(test_cluster_two_shards, currentDatabase(), dist2_layer_01071, key%2)
