CREATE TABLE test_table
(
    `eventType` String,
    `timestamp` UInt64,
    `key` UInt64
)
ENGINE = ReplacingMergeTree
PRIMARY KEY (eventType, timestamp)
ORDER BY (eventType, timestamp, key)
SETTINGS index_granularity = 1
