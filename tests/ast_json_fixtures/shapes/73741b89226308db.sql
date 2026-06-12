REPLACE TABLE t
(
    `timestamp` DateTime,
    `id` String,
    `value` String,
)
ENGINE = MergeTree
ORDER BY (id, toStartOfDay(timestamp))
TTL timestamp + toIntervalDay(1)
    GROUP BY id, toStartOfDay(timestamp)
    SET timestamp = max(timestamp) + interval 100 years, id = max(value)
