CREATE TABLE test_block_mismatch_sk1
(
    a UInt32,
    b DateTime
)
ENGINE = ReplacingMergeTree
PARTITION BY toYYYYMM(b)
PRIMARY KEY (toDate(b))
ORDER BY (toDate(b), a)
