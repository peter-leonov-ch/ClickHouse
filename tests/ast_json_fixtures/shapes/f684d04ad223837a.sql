WITH excludes AS (
    SELECT 1 as id, ['b','d'] AS exclude
    UNION ALL
    SELECT 2 as id, ['a','c']
    UNION ALL
    SELECT 3 as id, ['x','y']
)
SELECT
    id, arrayExcept(['a','b','c'], exclude) AS result
FROM excludes ORDER BY id
