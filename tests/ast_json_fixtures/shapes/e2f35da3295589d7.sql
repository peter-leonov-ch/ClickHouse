CREATE TABLE test_block_mismatch_sk2
(
    a UInt32,
    b DateTime
)
ENGINE = ReplacingMergeTree
PARTITION BY toYYYYMM(b)
PRIMARY KEY (a)
ORDER BY (a, toDate(b))
