SELECT l.number, r.number % 3 AS mod3, sum(r.number),
       grouping(l.number), grouping(mod3)
FROM numbers(1) l JOIN numbers(2) r ON l.number < r.number
GROUP BY ALL WITH CUBE
