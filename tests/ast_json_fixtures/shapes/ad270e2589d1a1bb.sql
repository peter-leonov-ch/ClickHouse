CREATE TABLE t_sample_factor(a UInt64, b UInt64) ENGINE = MergeTree ORDER BY (a, b) SAMPLE BY b
