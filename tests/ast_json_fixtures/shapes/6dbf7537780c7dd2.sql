WITH subquery AS (SELECT sum(number) FROM numbers(10)) SELECT (SELECT * FROM subquery)
