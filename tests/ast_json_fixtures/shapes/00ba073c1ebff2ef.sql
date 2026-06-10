SELECT 4, count(4) IGNORE NULLS, number % 2 AS key FROM numbers(10) GROUP BY key WITH ROLLUP WITH TOTALS QUALIFY key = materialize(0)
