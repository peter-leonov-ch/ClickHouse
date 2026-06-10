EXPLAIN PIPELINE
SELECT a
FROM t
GROUP BY a
FORMAT PrettySpace
SETTINGS optimize_aggregation_in_order = 1
