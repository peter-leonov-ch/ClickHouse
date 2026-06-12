SELECT NULL FROM t_02709__fuzz_23 FINAL
GROUP BY sign, '1023'
ORDER BY nan DESC, [0, NULL, NULL, NULL, NULL] DESC
FORMAT Null
SETTINGS
    max_parallel_replicas = 3,
    enable_parallel_replicas = 1,
    use_hedged_requests = 0,
    cluster_for_parallel_replicas = 'test_cluster_one_shard_three_replicas_localhost'
