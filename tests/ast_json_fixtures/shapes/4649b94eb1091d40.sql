create table dist_layer_01223 as data_01223 Engine=Distributed(test_shard_localhost, currentDatabase(), data_01223)
