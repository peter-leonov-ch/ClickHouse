WITH filtered AS (
    SELECT id, tup
    FROM tuple_test
    WHERE tup IS NOT NULL
)
SELECT id, tup.u, tup.s
FROM filtered
ORDER BY id
SETTINGS enable_analyzer = 1
