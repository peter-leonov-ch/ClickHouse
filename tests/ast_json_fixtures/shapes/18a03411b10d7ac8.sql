SELECT id, max(val) FROM 03408_dist GROUP BY id ORDER BY id LIMIT 1 BY id LIMIT 4
FORMAT JSONCompact SETTINGS max_block_size=1, exact_rows_before_limit = 1, distributed_group_by_no_merge=2
