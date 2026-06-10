SELECT CounterID, uniq(UserID) FROM test.hits WHERE 0 != 0 GROUP BY CounterID
