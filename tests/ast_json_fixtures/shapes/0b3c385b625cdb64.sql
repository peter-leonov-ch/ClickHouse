CREATE TABLE database_123456789abcde.tbl
(
    a UInt64,
    b UInt64,
    INDEX mmi_idx b TYPE minmax
)
ENGINE = MergeTree
PRIMARY KEY a
SETTINGS add_minmax_index_for_numeric_columns=0
