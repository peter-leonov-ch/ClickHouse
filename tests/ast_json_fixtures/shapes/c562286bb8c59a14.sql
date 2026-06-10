CREATE TABLE table4 (id Int64, v UInt64, s String,
INDEX a (id * 2, s) TYPE minmax GRANULARITY 3
) ENGINE = MergeTree() PARTITION BY id % 10 ORDER BY v
