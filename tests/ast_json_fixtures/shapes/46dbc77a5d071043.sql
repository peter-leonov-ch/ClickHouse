SELECT CounterID, uniq(UserID) FROM test.hits WHERE 0 != 0 GROUP BY CounterID SETTINGS optimize_aggregation_in_order = 1
