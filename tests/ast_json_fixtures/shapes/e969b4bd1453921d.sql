CREATE TABLE numbers_indexed Engine=MergeTree ORDER BY number PARTITION BY bitShiftRight(number,8) SETTINGS index_granularity=8 AS SELECT * FROM numbers(16384)
