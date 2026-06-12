SELECT count() AS c FROM test.hits WHERE CounterID = 1704509 WITH TOTALS SETTINGS totals_mode = 'after_having_auto',      max_rows_to_group_by = 100000, group_by_overflow_mode = 'any'
