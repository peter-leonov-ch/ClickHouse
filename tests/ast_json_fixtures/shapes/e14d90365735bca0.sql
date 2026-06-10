CREATE TABLE t_subcolumns_dist AS t_subcolumns_local ENGINE = Distributed(test_cluster_two_shards, currentDatabase(), t_subcolumns_local)
