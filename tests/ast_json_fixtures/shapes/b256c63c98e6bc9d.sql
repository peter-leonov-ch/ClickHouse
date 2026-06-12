SELECT sum(u) AS value FROM t_sparse_full GROUP BY id % 3 AS k WITH CUBE ORDER BY value
