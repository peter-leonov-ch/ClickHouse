SELECT (SELECT count() FROM system.one WHERE number = 2) FROM numbers(2) GROUP BY number % 2
