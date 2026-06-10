SELECT id, category, value, row_number() OVER (PARTITION BY category ORDER BY value) AS rn
FROM test_limit_by_all_old_planner
ORDER BY id, category, value, rn 
LIMIT 1 BY id, category, value, rn LIMIT 3
