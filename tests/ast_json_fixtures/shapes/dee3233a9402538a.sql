CREATE TABLE t (x String) ENGINE = MergeTree ORDER BY cityHash64(x) SAMPLE BY cityHash64(x)
