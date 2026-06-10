SELECT COUNT() = 10 FROM test_table WHERE day = '2020-01-01' UNION ALL SELECT 1 FROM numbers(1) SETTINGS max_rows_to_read = 11
