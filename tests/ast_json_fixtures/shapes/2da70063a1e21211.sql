SELECT 4
FROM
(
    SELECT
        1 AS X,
        2 AS Y
    UNION ALL
    SELECT
        3,
        4
    GROUP BY 2
)
WHERE materialize(4)
ORDER BY materialize(4) ASC NULLS LAST
