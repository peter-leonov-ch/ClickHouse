EXPLAIN PIPELINE
SELECT *
FROM tab
ORDER BY t ASC
SETTINGS read_in_order_two_level_merge_threshold = 0, max_threads = 4, read_in_order_use_buffering = 0
FORMAT tsv
