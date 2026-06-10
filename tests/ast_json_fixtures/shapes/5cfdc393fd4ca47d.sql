CREATE TABLE source_data (
    pk Int32, sk Int32, val UInt32, partition_key UInt32 DEFAULT 1,
    PRIMARY KEY (pk)
) ENGINE=MergeTree
ORDER BY (pk, sk)
