SELECT k, finalizeAggregation(uniqState(x)) FROM (SELECT intDiv(number, 3) AS k, toNullable(1) AS x FROM system.numbers LIMIT 100000) GROUP BY k WITH TOTALS ORDER BY k LIMIT 5
