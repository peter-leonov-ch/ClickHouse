CREATE TABLE cast2 AS cast1 ENGINE = ReplicatedMergeTree('/clickhouse/tables/{database}/test_00643/cast', 'r2') ORDER BY e
