(
    SELECT 1 x, x y FROM remote('localhost', currentDatabase(), t0)
)
UNION ALL
(
    SELECT 1, c1 FROM t0 SETTINGS cluster_for_parallel_replicas='test_cluster_one_shard_three_replicas_localhost', parallel_replicas_for_non_replicated_merge_tree = 1
)
