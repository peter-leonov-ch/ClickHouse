SELECT 1, x, 2, s, 3, k, 4 FROM (SELECT number AS k FROM system.numbers LIMIT 10) js1 ANY LEFT JOIN t2 USING k
