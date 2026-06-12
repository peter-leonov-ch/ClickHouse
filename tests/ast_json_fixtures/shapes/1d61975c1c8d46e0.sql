SELECT id, category, count(*) as cnt
FROM test_limit_by_all 
GROUP BY id, category
ORDER BY id, category 
LIMIT 1 BY ALL
