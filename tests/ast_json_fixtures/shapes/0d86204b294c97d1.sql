SELECT sum(c0 = 0), min(c0 + 1), sum(c0 + 2) FROM t_having
GROUP BY c0 HAVING c0 = 0
