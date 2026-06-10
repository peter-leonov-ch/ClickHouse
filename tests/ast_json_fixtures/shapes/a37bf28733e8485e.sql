WITH
    toDateTime64('2024-12-12 12:00:10', 3, 'UTC') AS start_ts,
    toDateTime64('2024-12-12 12:01:00', 3, 'UTC') AS end_ts,
    10 AS step_sec,
    50 as window_sec,
    range(toUnixTimestamp(start_ts), toUnixTimestamp(end_ts) + 1, step_sec) as grid
SELECT metric_id, arrayJoin(arrayZip(grid, irate_values, idelta_values, rate_values, delta_values))
FROM (
    SELECT
        metric_id,
        timeSeriesInstantRateToGrid(start_ts, end_ts, step_sec, window_sec)(samples.1, samples.2) as irate_values,
        timeSeriesInstantDeltaToGrid(start_ts, end_ts, step_sec, window_sec)(samples.1, samples.2) as idelta_values,
        timeSeriesRateToGrid(start_ts, end_ts, step_sec, window_sec)(samples.1, samples.2) as rate_values,
        timeSeriesDeltaToGrid(start_ts, end_ts, step_sec, window_sec)(samples.1, samples.2) as delta_values
    FROM clusterAllReplicas('test_shard_localhost', currentDatabase(), t_resampled_timeseries_64)
    GROUP BY metric_id
)
ORDER BY metric_id
SETTINGS enable_parallel_replicas=1, max_parallel_replicas=3, parallel_replicas_for_non_replicated_merge_tree=1, enable_analyzer=1
