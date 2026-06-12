WITH t AS
    (
        SELECT number + a AS x
        FROM numbers(5)
    )
SELECT *
FROM
(
    SELECT
        0 AS a,
        x
    FROM t
    UNION ALL
    SELECT
        5 AS a,
        x
    FROM t
)
ORDER BY
    a ASC,
    x ASC
SETTINGS enable_analyzer = 1
