SELECT toBool(sin(SUM(number))) AS x
FROM
(
    SELECT 1 AS number
)
GROUP BY number
HAVING 1 AND sin(1)
ORDER BY ALL
SETTINGS enable_optimize_predicate_expression = 0
