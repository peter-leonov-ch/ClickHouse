SELECT 1 FROM remote('127.0.0.{1,1}') GROUP BY (2, materialize(3)) HAVING materialize(3) SETTINGS group_by_use_nulls = true
