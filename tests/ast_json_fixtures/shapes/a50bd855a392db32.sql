CREATE MATERIALIZED VIEW test_mv_pk
(
    value String,
    id UInt64
) ENGINE=MergeTree PRIMARY KEY value
POPULATE AS SELECT value, id FROM test
