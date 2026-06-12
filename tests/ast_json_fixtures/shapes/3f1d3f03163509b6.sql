WITH toStartOfHour(ts) AS a SELECT sum(value) v FROM agg WHERE ts > '2021-12-06 22:00:00' GROUP BY a ORDER BY v LIMIT 5
