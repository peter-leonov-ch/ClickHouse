SELECT
    'limit w/ GROUP BY',
    count(),
    number
FROM remote('127.{1,2}', view(
    SELECT intDiv(number, 2) AS number
    FROM numbers(10)
))
GROUP BY number
ORDER BY
    count() ASC,
    number DESC
SETTINGS limit=2
