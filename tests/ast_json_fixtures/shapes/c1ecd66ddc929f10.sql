SELECT id, category, value, row_number() OVER (PARTITION BY category ORDER BY value) AS rn
FROM test_limit_by_all
ORDER BY id, category, value, rn 
LIMIT 1 BY ALL LIMIT 3
