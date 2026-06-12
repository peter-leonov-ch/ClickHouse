SELECT id, category, value 
FROM test_limit_by_all 
WHERE value > 200
ORDER BY id, category, value  
LIMIT 1 BY ALL
