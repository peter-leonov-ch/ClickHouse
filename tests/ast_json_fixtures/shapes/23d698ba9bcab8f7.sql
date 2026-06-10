INSERT INTO insert_select_dst(COLUMNS('.*') EXCEPT (middle_a, middle_b)) SELECT * FROM insert_select_src
