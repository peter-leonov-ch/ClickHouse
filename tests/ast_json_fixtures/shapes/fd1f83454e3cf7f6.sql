SELECT (number % 2) AS key, count() FROM numbers(10) GROUP BY key QUALIFY key == 0
