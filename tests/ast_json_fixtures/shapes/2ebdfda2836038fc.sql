WITH
  t as (SELECT number + a as x FROM numbers(5))
SELECT 0 as a, x FROM t
UNION ALL
SELECT 5 as a, x FROM t
ORDER BY a, x
FORMAT Null
SETTINGS enable_analyzer = 1
