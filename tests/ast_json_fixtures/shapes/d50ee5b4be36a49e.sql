SELECT
    ProfileEvents['FileOpen'],
    read_rows,
    arraySort(arrayMap(x -> splitByChar('.', x)[-1], columns))
FROM system.query_log
WHERE type = 'QueryFinish'
    AND current_database = currentDatabase()
    AND query LIKE '%SELECT sum(b) FROM t_index_hint%'
ORDER BY event_time_microseconds
