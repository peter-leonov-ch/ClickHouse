WITH stats AS (
    SELECT
        avg(tup.u) as avg_u,
        max(tup.u) as max_u
    FROM tuple_test
)
SELECT
    id,
    tup.u,
    tup.u - (SELECT avg_u FROM stats) as diff_from_avg
FROM tuple_test
WHERE tup IS NOT NULL
ORDER BY id
