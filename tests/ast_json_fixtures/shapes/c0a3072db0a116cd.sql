SELECT id, category, value, name
FROM test_limit_by_all_old_planner
LIMIT 1 BY ALL
SETTINGS allow_experimental_analyzer = 0
