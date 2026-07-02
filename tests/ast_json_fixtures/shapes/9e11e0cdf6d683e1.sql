SELECT used_functions
FROM system.query_log WHERE current_database = currentDatabase() AND type != 'QueryStart' AND (query LIKE '%repeat%')
ORDER BY query_start_time DESC LIMIT 1 FORMAT TabSeparatedWithNames
