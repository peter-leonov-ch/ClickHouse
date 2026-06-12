SELECT number % 2 AS key, count(materialize(5)) IGNORE NULLS FROM numbers(10) WHERE toLowCardinality(toLowCardinality(materialize(2))) GROUP BY key WITH CUBE WITH TOTALS QUALIFY key = 0
