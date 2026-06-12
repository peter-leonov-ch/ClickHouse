SELECT
  n as m,
  count() OVER (PARTITION BY m) cnt
FROM t
WHERE st IN ('x', 'y')
ORDER BY ALL
LIMIT 1 BY m
