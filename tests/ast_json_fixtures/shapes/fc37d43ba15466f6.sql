WITH toStartOfHour(toDateTime('2025-01-01 12:00:00')) AS h
SELECT h, category
FROM test_limit_by_all
ORDER BY h, category, value
LIMIT 1 BY ALL
SETTINGS enable_positional_arguments = 0
