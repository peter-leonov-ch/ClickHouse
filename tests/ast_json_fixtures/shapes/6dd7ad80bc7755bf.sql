SELECT * FROM system.query_log WHERE event_date >= yesterday() AND current_database = currentDatabase() AND memory_usage > 100e6 FORMAT JSONEachRow
