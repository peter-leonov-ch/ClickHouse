CREATE TABLE foo_with_projection (ts DateTime, x UInt64, PROJECTION pj (SELECT ts, x ORDER BY x))
    ENGINE = MergeTree PARTITION BY toYYYYMMDD(ts) ORDER BY (ts)
