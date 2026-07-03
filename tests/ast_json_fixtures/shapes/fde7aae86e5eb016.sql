CREATE TABLE t02006 ON CLUSTER test_shard_localhost
(
    `a` String,
    `b` UInt32
)
ENGINE = ReplicatedMergeTree
PRIMARY KEY a
ORDER BY a
format Null
