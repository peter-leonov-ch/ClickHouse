CREATE TABLE data_01875_1 Engine=MergeTree ORDER BY number PARTITION BY bitShiftRight(number, 8) + 1 AS SELECT * FROM numbers(16384)
