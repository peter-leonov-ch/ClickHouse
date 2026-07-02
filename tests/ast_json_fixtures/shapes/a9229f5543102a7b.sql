select * from (select * from cluster(test_cluster_two_shards, currentDatabase(), test) where i < 10) group by i order by i limit 10 FORMAT JSONCompact
