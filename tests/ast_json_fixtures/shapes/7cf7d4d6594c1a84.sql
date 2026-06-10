SELECT  materialize('a'), 'a' AS key GROUP BY key WITH CUBE WITH TOTALS SETTINGS group_by_use_nulls = 1
