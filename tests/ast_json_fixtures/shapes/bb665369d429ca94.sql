SELECT (number % 2) AS key, count() FROM numbers(10) GROUP BY key HAVING key = 0 QUALIFY key == 0
