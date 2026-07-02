SELECT *
FROM
(
    SELECT *
    FROM system.numbers
    WHERE number = 100
    UNION ALL
    SELECT *
    FROM system.numbers
    WHERE number = 100
)
LIMIT 2
SETTINGS max_threads = 1 FORMAT Null
