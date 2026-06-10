WITH
(
    SELECT
        (query_id, query_start_time, query_start_time_microseconds)
    FROM
        system.query_log
    WHERE
          event_date >= yesterday()
      AND current_database = currentDatabase()
      AND log_comment = '02985_shard_query_start_time_query_1'
      AND type = 'QueryFinish'
) AS id_and_start_tuple
SELECT
    type,
    countIf(query_start_time >= initial_query_start_time), 
    countIf(query_start_time_microseconds > initial_query_start_time_microseconds),
    countIf(initial_query_start_time = id_and_start_tuple.2),
    countIf(initial_query_start_time_microseconds = id_and_start_tuple.3)
FROM
    system.query_log
WHERE
    NOT is_initial_query AND initial_query_id = id_and_start_tuple.1
GROUP BY type
