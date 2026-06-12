SELECT arrayMap(x -> '.', range(number % 10)) AS k FROM remote('127.0.0.{2,3}', numbers(10)) GROUP BY GROUPING SETS ((k)) ORDER BY k settings group_by_use_nulls=1
