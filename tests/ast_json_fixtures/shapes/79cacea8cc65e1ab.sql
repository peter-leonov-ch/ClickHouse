CREATE TABLE dist AS local_table ENGINE = Distributed(test_cluster_two_shards_localhost, currentDatabase(), local_table)
