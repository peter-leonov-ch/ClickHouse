SELECT DISTINCT a
FROM distinct_in_order
ORDER BY a ASC
SETTINGS read_in_order_two_level_merge_threshold = 0,
optimize_read_in_order = 1,
max_threads = 2
