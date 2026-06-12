CREATE TABLE t_dict_dist AS t_dict_dist_local
ENGINE = Distributed(test_shard_localhost, currentDatabase(), t_dict_dist_local)
