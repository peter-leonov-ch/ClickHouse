WITH
    '2024-12-12 12:00:15'::DateTime64(3,'UTC') AS start_ts,       
    start_ts + interval 60 second AS end_ts,   
    15 AS step_seconds,   
    45 AS window_seconds  
SELECT
    metric_id,
    timeSeriesInstantDeltaToGrid(start_ts, end_ts, step_seconds, window_seconds)(timestamp, value),
    timeSeriesInstantRateToGrid(start_ts, end_ts, step_seconds, window_seconds)(timestamp, value)
FROM t_raw_timeseries
WHERE metric_id = 3 AND timestamp BETWEEN start_ts - interval window_seconds seconds AND end_ts
GROUP BY metric_id
