CREATE TABLE visits_dist AS visits ENGINE Distributed(test_cluster_two_shards_localhost,  currentDatabase(), 'visits', rand())
