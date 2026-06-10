CREATE MATERIALIZED VIEW mv_primary_key
(
    key String,
    PRIMARY KEY key
)
ENGINE = MergeTree
AS SELECT * FROM data
