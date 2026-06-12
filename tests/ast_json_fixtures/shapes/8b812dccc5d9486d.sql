WITH CAST(tuple(1), 'Tuple (value UInt64)') AS compound_value
SELECT id, test_table.* APPLY x -> compound_value.*
FROM test_table
WHERE arrayMap(x -> toString(x) AS lambda, [NULL, 256, 257, NULL, NULL])
SETTINGS convert_query_to_cnf = true, optimize_using_constraints = true, optimize_substitute_columns = true
