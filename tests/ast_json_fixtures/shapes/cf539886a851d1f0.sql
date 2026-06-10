CREATE TABLE full_duplicates  (
    pk Int32, sk Int32, val UInt32, partition_key UInt32, mat UInt32 MATERIALIZED 12345, alias UInt32 ALIAS 2,
    PRIMARY KEY (pk)
) ENGINE=MergeTree
PARTITION BY (partition_key + 1) 
ORDER BY (pk, toString(sk * 10))
