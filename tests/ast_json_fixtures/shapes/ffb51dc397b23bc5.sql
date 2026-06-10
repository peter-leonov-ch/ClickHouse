CREATE TEMPORARY TABLE IF NOT EXISTS tmp_a
(
    k1 Int32,
    k2 Int32,
    d1 Int32,
    d2 Int32
) ENGINE = MergeTree ORDER BY tuple()
