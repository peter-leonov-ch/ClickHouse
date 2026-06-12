CREATE TABLE derived_metrics_local
(
  timestamp DateTime,
  bytes UInt64
)
ENGINE=SummingMergeTree()
PARTITION BY toYYYYMMDD(timestamp)
ORDER BY (toStartOfHour(timestamp), timestamp)
TTL toStartOfHour(timestamp) + INTERVAL 1 HOUR GROUP BY toStartOfHour(timestamp)
SET bytes=max(bytes)
