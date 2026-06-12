SELECT 'pending to flush', length(entries.bytes) FROM system.asynchronous_inserts
WHERE database = currentDatabase() AND table = 't_async_insert_skip_settings'
ORDER BY first_update
