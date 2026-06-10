CREATE TABLE test_r1 AS test ENGINE = ReplicatedMergeTree('/clickhouse/{database}/test_01666', 'r1') ORDER BY "\\" SETTINGS min_bytes_for_wide_part = '100G', replace_long_file_name_to_hash = 1
