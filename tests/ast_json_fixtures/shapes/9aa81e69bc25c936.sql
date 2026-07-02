CREATE MATERIALIZED VIEW with_deduplication_mv UUID '00000510-1000-4000-8000-000000000001'
    ENGINE = ReplicatedAggregatingMergeTree('/clickhouse/tables/{database}/test_00510/with_deduplication_mv', 'r1') ORDER BY dummy
    AS SELECT 0 AS dummy, countState(x) AS cnt FROM with_deduplication
