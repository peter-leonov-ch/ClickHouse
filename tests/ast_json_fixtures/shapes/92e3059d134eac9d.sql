CREATE TABLE t0 (
    key Int32,
    value Int32
)
ENGINE=MergeTree()
PRIMARY KEY key
PARTITION BY key % 2
