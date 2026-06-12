SELECT count() FROM test PREWHERE x >= 10 WHERE x < 11 AND y = 10 SETTINGS max_rows_to_read = 3
