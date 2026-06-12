WITH (
        SELECT uuid
        FROM system.tables
        WHERE (database = currentDatabase()) AND (name = '02581_trips')
    ) AS table_uuid
SELECT
    CAST(splitByChar('_', query_id)[5], 'UInt64') AS mutation_version, 
    sum(message LIKE 'Created Set with % entries%') >= 1  AS has_parts_for_which_set_was_built,
    sum(message LIKE 'Got set from cache%') >= 1 AS has_parts_that_shared_set
FROM system.text_log
WHERE
    query_id LIKE concat(CAST(table_uuid, 'String'), '::all\\_%')
    AND (event_date >= yesterday())
    AND (message LIKE 'Created Set with % entries%' OR message LIKE 'Got set from cache%')
GROUP BY mutation_version ORDER BY mutation_version FORMAT TSVWithNames
