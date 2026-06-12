CREATE TABLE t0 (c0 Int, c1 Nullable(Int) MATERIALIZED 1) ENGINE = SummingMergeTree() PRIMARY KEY (abs(c1)) SETTINGS allow_nullable_key = 1
