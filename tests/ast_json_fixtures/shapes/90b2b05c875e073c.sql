CREATE TABLE pr_1 (`a` UInt32) ENGINE = MergeTree ORDER BY a PARTITION BY a % 10 AS
SELECT 10 * intDiv(number, 10) + 1 FROM numbers(1_000)
