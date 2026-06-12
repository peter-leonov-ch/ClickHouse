SELECT x, count() FROM t_mutation_rows_counter GROUP BY x HAVING count() > 1
