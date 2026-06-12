SELECT id AS k, category
FROM test_limit_by_all
ORDER BY k, category, value
LIMIT 1 BY ALL
