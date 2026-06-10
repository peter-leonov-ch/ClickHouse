WITH cast(tuple(1), 'Tuple (value UInt64)') AS compound_value SELECT id, test_table.* APPLY x -> compound_value.* FROM test_table
