SELECT a + 1 AS a, b AS b, c + 1 AS c, d + 1 AS d FROM t_lazy_mat_prewhere_parallel PREWHERE d > 1 ORDER BY c LIMIT 3
