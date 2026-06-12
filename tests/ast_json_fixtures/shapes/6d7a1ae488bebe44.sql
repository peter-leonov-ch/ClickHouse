SELECT arraySort(used_data_type_families), used_storages
FROM system.query_log
WHERE current_database = currentDatabase() AND type == 'QueryFinish' AND (query LIKE '%TABLE test%')
ORDER BY query_start_time DESC LIMIT 1 FORMAT TabSeparatedWithNames
