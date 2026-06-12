SELECT x, sum(t2.x) FROM t1 JOIN t1 as t2 ON t1.x = t2.x GROUP BY t1.x ORDER BY ALL
