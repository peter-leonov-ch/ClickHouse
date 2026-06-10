CREATE MATERIALIZED VIEW mv_projections
(
    key String,
    projection p (SELECT uniqCombined(key))
)
ENGINE = MergeTree
ORDER BY key
AS SELECT * FROM data
