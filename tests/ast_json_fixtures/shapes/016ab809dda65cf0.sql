SELECT c0 + 1 as expr FROM test_limit_by_validation GROUP BY c0 + 1 ORDER BY expr LIMIT 1 BY expr
