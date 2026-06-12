CREATE TABLE t
(
  tid UInt64,
  processed_at DateTime,
  created_at DateTime,
  amount Int64
)
ENGINE = ReplacingMergeTree
PARTITION BY toStartOfQuarter(created_at)
PRIMARY KEY (toStartOfDay(created_at), toStartOfDay(processed_at))
ORDER BY (toStartOfDay(created_at), toStartOfDay(processed_at), tid)
SETTINGS index_granularity = 1
