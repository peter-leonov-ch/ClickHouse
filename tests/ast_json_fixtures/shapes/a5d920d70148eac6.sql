CREATE TABLE test_index
(
    `key_string` String,
    `key_uint32` ALIAS toUInt32(key_string),
    INDEX idx toUInt32(key_string) TYPE set(0) GRANULARITY 1
)
ENGINE = MergeTree
PARTITION BY tuple()
PRIMARY KEY tuple()
ORDER BY key_string SETTINGS index_granularity = 1
