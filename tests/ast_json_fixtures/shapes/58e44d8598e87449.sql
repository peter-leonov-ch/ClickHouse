CREATE TABLE column_swap_test_test (i Int64, a String, b UInt64, CONSTRAINT c1 ASSUME b = cityHash64(a))
ENGINE = MergeTree() ORDER BY i
SETTINGS min_bytes_for_wide_part = 0
