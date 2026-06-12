SELECT id % 3 AS k, sum(u) FROM t_sparse_full WHERE u != 0 GROUP BY k ORDER BY k
