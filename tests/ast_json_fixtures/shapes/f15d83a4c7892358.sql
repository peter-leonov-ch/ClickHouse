CREATE TEMPORARY TABLE table_merge_tree_02525
(
    id UInt64,
    info String
)
ENGINE = MergeTree
ORDER BY id
PRIMARY KEY id
