EXPLAIN PIPELINE SELECT * FROM data FINAL WHERE v1 >= now() - INTERVAL 180 DAY
SETTINGS max_threads=2, max_final_threads=2, force_data_skipping_indices='v1_index', use_skip_indexes_if_final=0
FORMAT LineAsString
