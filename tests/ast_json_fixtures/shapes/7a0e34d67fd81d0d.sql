SELECT arraySort(used_table_functions)
FROM system.query_log WHERE current_database = currentDatabase() AND type = 'QueryFinish' AND (query LIKE '%toDate(\'2000-12-05\')%')
ORDER BY query_start_time DESC LIMIT 1 FORMAT TabSeparatedWithNames
