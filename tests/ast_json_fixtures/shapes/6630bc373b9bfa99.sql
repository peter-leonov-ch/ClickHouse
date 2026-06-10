SELECT _CAST(NULL, 'Nullable(Nothing)') AS `NULL`
FROM t_02709__fuzz_23 FINAL
GROUP BY
    t_02709__fuzz_23.sign,
    '1023'
ORDER BY
    nan DESC,
    _CAST([0, NULL, NULL, NULL, NULL], 'Array(Nullable(UInt8))') DESC
FORMAT Null
SETTINGS receive_timeout = 10., receive_data_timeout_ms = 10000, use_hedged_requests = 0, allow_suspicious_low_cardinality_types = 1, max_parallel_replicas = 3, cluster_for_parallel_replicas = 'test_cluster_one_shard_three_replicas_localhost', enable_parallel_replicas = 1, parallel_replicas_for_non_replicated_merge_tree = 1, log_queries = 1, table_function_remote_max_addresses = 200, enable_analyzer = 1
