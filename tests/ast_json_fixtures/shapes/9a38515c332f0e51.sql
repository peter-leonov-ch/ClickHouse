SELECT * FROM numbers(4) GROUP BY number WITH TOTALS HAVING sum(number) <= arrayJoin([])
