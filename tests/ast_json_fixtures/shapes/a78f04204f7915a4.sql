CREATE TABLE 03400_dist_users AS 03400_users
ENGINE = Distributed(test_cluster_two_shards, currentDatabase(), 03400_users)
