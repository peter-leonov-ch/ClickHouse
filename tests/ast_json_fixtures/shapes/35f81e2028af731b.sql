WITH dummy + 3 AS dummy
SELECT dummy + 1 AS y
FROM system.one
SETTINGS enable_global_with_statement = 1
