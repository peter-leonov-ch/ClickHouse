INSERT INTO t_insert_select_parens (x) (SELECT * FROM numbers(5)) EXCEPT (SELECT * FROM numbers(3))
