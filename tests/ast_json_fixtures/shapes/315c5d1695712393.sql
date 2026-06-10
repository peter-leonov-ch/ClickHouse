CREATE TABLE tab2 ENGINE=ReplacingMergeTree ORDER BY n SETTINGS add_minmax_index_for_numeric_columns=0 AS SELECT intDiv(number,2) as n from numbers(8192 * 123)
