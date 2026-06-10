CREATE TABLE t0
(
    c0 Int64,
    INDEX i1 c0 TYPE set(0)
) ENGINE = SummingMergeTree() PARTITION BY (c0) ORDER BY (c0)
