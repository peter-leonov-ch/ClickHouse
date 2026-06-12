CREATE TABLE distributed_00952 AS local_00952 ENGINE = Distributed('test_cluster_two_shards', currentDatabase(), local_00952, rand())
