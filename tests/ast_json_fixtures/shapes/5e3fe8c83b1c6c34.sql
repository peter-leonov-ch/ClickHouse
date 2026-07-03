SELECT 'SLEEP #1 CHECK', ProfileEvents['SleepFunctionCalls'] as calls, ProfileEvents['SleepFunctionMicroseconds'] as microseconds
FROM system.query_log
WHERE query like '%SELECT ''SLEEP #1 TEST''%'
  AND type > 1
  AND current_database = currentDatabase()
  AND event_date >= yesterday()
    FORMAT JSONEachRow
