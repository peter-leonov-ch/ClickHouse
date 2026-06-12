WITH RECURSIVE t AS (
    SELECT 1 AS id, []::Array(UInt64) AS path
UNION ALL
    SELECT tree.id, arrayConcat(t.path, [tree.id])
    FROM tree JOIN t ON (tree.parent_id = t.id)
)
SELECT t1.id, count(t2.path) FROM t AS t1 JOIN t AS t2 ON
    (t1.path[1] = t2.path[1] AND
    length(t1.path) = 1 AND
    length(t2.path) > 1)
    GROUP BY t1.id
    ORDER BY t1.id
