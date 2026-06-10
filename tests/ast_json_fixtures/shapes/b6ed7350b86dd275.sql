CREATE TABLE 03636_data_partitions (ts DateTime) ENGINE = MergeTree ORDER BY tuple() PARTITION BY toStartOfDay(ts)
AS
SELECT 1756882680
