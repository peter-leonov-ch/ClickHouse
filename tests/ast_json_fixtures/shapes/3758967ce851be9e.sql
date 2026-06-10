SELECT
    'implicit_True',
    count() as all,
    transaction_id = (0,0,'00000000-0000-0000-0000-000000000000') as is_empty
FROM system.query_log
WHERE
    current_database = currentDatabase() AND
    event_date >= yesterday() AND
    query LIKE '-- Verify that the transaction_id column is populated correctly%'
GROUP BY transaction_id
FORMAT JSONEachRow
