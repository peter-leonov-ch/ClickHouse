CREATE TABLE test_foo (
    `123_secure` Int8,
    `date` Date,
    `town` LowCardinality(String),
)
ENGINE = MergeTree
PRIMARY KEY (town, date)
PARTITION BY toYear(date)
COMMENT 'test' 
SETTINGS
    enforce_strict_identifier_format=true
