SELECT id, val FROM 03408_memory GROUP BY id, val HAVING id < 7 ORDER BY id, val DESC LIMIT 1 BY id LIMIT 3
FORMAT JsonCompact SETTINGS exact_rows_before_limit=1
