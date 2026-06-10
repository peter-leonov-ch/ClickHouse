create table dist_01643 as data_01643 engine=Distributed(test_cluster_two_shards, currentDatabase(), data_01643, key) settings fsync_after_insert=1, fsync_directories=1
