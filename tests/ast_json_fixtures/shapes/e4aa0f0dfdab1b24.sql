SELECT
    concat(current_database(), '')
FROM
(
    SELECT id
    FROM remote('127.0.0.1,127.0.0.2', currentDatabase(), test)
    LIMIT 0.2
)
ORDER BY ALL
FORMAT Null
