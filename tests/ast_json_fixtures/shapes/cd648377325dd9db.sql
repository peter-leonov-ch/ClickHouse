select count(), * from cluster(test_cluster_two_shards, currentDatabase(), dist_01247) group by number order by number settings distributed_group_by_no_merge=1
