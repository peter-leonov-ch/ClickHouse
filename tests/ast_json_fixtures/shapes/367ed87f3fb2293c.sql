SELECT 10
FROM system.query_log AS a
INNER JOIN system.processes AS b
ON (a.query_id = b.query_id) AND (a.query_id = b.query_id)
WHERE current_database = currentDatabase()
FORMAT Null
SETTINGS query_plan_use_new_logical_join_step = true, query_plan_convert_join_to_in = true
