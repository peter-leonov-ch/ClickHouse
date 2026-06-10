SELECT sum(u) FROM t_sparse_full GROUP BY id % 3 AS k WITH TOTALS ORDER BY k
