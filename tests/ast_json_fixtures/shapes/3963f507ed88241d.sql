SELECT number, number % 2, sum(number) AS val
FROM numbers(10)
GROUP BY CUBE(number, number % 2)
ORDER BY (number, number % 2, val)
SETTINGS group_by_use_nulls=0
