WITH
    '2024-12-12 12:00:15'::DateTime64(3,'UTC') AS start_ts,       
    start_ts + interval 60 second AS end_ts,   
    15 AS step_seconds,   
    45 AS window_seconds  
SELECT
    metric_id,
    timeSeriesInstantDeltaToGrid(start_ts, end_ts, step_seconds, window_seconds)(timestamps, values),
    timeSeriesInstantRateToGrid(start_ts, end_ts, step_seconds, window_seconds)(timestamps, values)
FROM (
    SELECT
        metric_id,
        finalizeAggregation(samples).1 as timestamps,
        finalizeAggregation(samples).2 as values
    FROM t_resampled_timeseries_15_sec
    WHERE metric_id = 3 AND grid_timestamp BETWEEN start_ts - interval window_seconds seconds AND end_ts
)
GROUP BY metric_id
