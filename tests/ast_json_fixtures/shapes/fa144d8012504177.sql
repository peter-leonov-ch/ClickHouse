SELECT * FROM table1 AS t1 ALL LEFT JOIN (SELECT *, '0.10', c, d AS b FROM table2) AS t2 USING (a, b) ORDER BY d, t1.a ASC FORMAT PrettyCompact settings max_rows_in_join = 1
