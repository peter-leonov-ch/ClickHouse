SELECT DISTINCT 2
FROM
    (SELECT 2 AS b, 1 AS a GROUP BY 1, toUInt256(1), 1 WITH CUBE WITH TOTALS) AS foo
  ANY LEFT JOIN
    (SELECT 2 AS b, 1 AS a) AS bar
  ON and(foo.b = bar.b, foo.a = bar.b)
WHERE 0
