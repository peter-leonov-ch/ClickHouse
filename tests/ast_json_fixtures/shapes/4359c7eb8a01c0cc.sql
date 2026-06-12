CREATE TABLE tab1 (id Int32, v Int32, INDEX secondaryidx v TYPE minmax) ENGINE=ReplacingMergeTree ORDER BY id SETTINGS index_granularity=2
