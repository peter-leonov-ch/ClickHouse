CREATE TABLE t
(
    i Int32
)
ENGINE = MergeTree
PARTITION BY i
ORDER BY tuple()
SETTINGS index_granularity = 1
