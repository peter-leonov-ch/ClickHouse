SELECT l.number, sum(r.number), grouping(l.number)
FROM numbers(1) l JOIN numbers(2) r ON l.number < r.number
GROUP BY ALL WITH ROLLUP
