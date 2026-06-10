SELECT id, category, concat(category, '_', name) as combined
FROM test_limit_by_all 
ORDER BY id, category, value  
LIMIT 2 BY ALL
