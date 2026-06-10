CREATE MATERIALIZED VIEW mv_no_indexes
(
    key String,
    INDEX idx key TYPE bloom_filter GRANULARITY 1
)
ENGINE = Null
AS SELECT * FROM data
