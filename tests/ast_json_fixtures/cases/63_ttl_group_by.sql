CREATE TABLE t (a UInt64, k UInt64, d Date) ENGINE = MergeTree ORDER BY (k, a) TTL d + INTERVAL 1 MONTH GROUP BY k SET a = max(a)
