SELECT length(arr) AS n, sum(v) FROM t_func_to_subcolumns_use_nulls GROUP BY n WITH ROLLUP HAVING n <= 4 OR isNull(n) ORDER BY n SETTINGS group_by_use_nulls = 1
