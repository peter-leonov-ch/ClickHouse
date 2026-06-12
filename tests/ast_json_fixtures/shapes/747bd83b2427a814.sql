SELECT number % toUInt256(2) AS key, count() FROM numbers(10) GROUP BY key WITH CUBE WITH TOTALS QUALIFY key = toNullable(toNullable(0))
