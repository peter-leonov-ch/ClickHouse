CREATE TABLE `test_foo` (
    `insecure_$` Int8,
    `date` Date,
    `town` LowCardinality(String),
)
ENGINE = MergeTree
PRIMARY KEY (town, date)
PARTITION BY toYear(date)
