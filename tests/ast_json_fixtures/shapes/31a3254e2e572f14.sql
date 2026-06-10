SELECT key, value1, value2, toUInt64(min(time)) AS start_ts FROM join_inner_table__fuzz_146 GROUP BY key, value1, value2 WITH CUBE ORDER BY key ASC NULLS LAST, value2 DESC NULLS LAST LIMIT 9223372036854775806
    FORMAT Null
    SETTINGS
        max_parallel_replicas = 3,
        prefer_localhost_replica = 1,
        cluster_for_parallel_replicas = 'test_cluster_one_shard_three_replicas_localhost',
        enable_parallel_replicas = 1,
        use_hedged_requests = 0
