WITH
    100 as begin,
    200 as end,
    10 as step_sec,
    15 as staleness_sec,
    CAST(begin as DateTime('UTC')) as begin_ts,
    CAST(end as DateTime('UTC')) as end_ts,
    range(begin, end+step_sec, step_sec) as grid
SELECT
   arrayZip(
       grid,
       timeSeriesResampleToGridWithStaleness(begin_ts, end_ts, step_sec, staleness_sec)(timestamp::DateTime64(6, 'UTC'), value)
   ) as e
FROM clusterAllReplicas('test_shard_localhost', currentDatabase(), ts_data) SETTINGS enable_parallel_replicas=1, max_parallel_replicas=3, parallel_replicas_for_non_replicated_merge_tree=1, prefer_localhost_replica = 0
