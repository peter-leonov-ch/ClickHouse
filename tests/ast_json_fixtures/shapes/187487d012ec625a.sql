CREATE TABLE index_append_test_test (i Int64, a UInt32, b UInt64, CONSTRAINT c1 ASSUME i <= 2 * b AND i + 40 > a) ENGINE = MergeTree() ORDER BY i
