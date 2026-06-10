WITH
    (
        SELECT query_id
        FROM system.query_log
        WHERE current_database = currentDatabase() AND (normalizeQuery(query) like normalizeQuery('WITH 01091 AS id SELECT 1;')) AND (event_date >= (today() - 1))
        ORDER BY event_time DESC
        LIMIT 1
    ) AS id
SELECT uniqExact(thread_id)
FROM system.query_thread_log
WHERE (event_date >= (today() - 1)) AND (query_id = id) AND (thread_id != master_thread_id)
