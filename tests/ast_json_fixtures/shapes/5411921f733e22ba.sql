select * from (select * from cluster(test_cluster_two_shards, currentDatabase(), test) where i < 10) order by i limit 1 FORMAT JSONCompact
