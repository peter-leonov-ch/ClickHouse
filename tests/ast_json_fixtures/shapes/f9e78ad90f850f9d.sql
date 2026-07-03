SELECT *
FROM table1
INNER JOIN table2
    ON (a = c)
WHERE (a > 0) AND (c > 0)
  AND ( ((a > 5) AND (c < 10)) OR ((a > 6) AND (c < 11)) )
ORDER BY a, c
FORMAT TSV
