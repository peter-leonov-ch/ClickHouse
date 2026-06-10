SELECT id, category, value 
FROM test_limit_by_all_old_planner 
WHERE value > 200
ORDER BY id, category, value  
LIMIT 1 BY id, category, value
