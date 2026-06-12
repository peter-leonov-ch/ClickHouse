SELECT *
FROM
(
    SELECT
        name,
        age
    FROM users
)
GROUP BY
    1,
    2
ORDER BY ALL
