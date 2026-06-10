CREATE TABLE users (uid Int16, d DateTime('UTC'))
ENGINE = MergeTree ORDER BY uid TTL d + INTERVAL 1 MONTH WHERE uid = 1
SETTINGS merge_with_ttl_timeout = 0, min_bytes_for_wide_part = 0, vertical_merge_algorithm_min_rows_to_activate = 0, vertical_merge_algorithm_min_columns_to_activate = 0
