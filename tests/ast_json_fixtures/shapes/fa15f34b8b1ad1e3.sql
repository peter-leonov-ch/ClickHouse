create table dp as d Engine=Distributed(test_cluster_two_shards, currentDatabase(), d, i)
