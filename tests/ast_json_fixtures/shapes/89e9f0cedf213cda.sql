CREATE TABLE IF NOT EXISTS normal
(
    `key` UInt32,
    `value` UInt32,
)
ENGINE = MergeTree
ORDER BY tuple() settings index_granularity=1
