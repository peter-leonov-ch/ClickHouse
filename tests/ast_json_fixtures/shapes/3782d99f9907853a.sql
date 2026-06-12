CREATE MATERIALIZED VIEW mv_primary_key_from_column
(
    key String PRIMARY KEY
)
ENGINE = MergeTree
AS SELECT * FROM data
