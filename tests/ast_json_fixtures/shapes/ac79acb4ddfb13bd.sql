CREATE TABLE test_bf_cast (c Int32, INDEX x1 (c) type bloom_filter) ENGINE = MergeTree ORDER BY c AS SELECT 1
