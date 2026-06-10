CREATE TABLE tab
(
    `uint64` UInt64,
    `int32` Nullable(Int32) COMMENT 'example comment',
    `str` String,
    INDEX idx str TYPE set(1000)
)
ENGINE = MergeTree
PRIMARY KEY (uint64)
ORDER BY (uint64, str)
