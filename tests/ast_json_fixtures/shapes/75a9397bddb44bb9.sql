create table distr as local engine = Distributed('test_cluster_two_shards', currentDatabase(), local)
