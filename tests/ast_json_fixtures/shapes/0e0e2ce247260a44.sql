create table dist_02175 as local_02175 engine=Distributed(test_cluster_two_shards, currentDatabase(), local_02175)
