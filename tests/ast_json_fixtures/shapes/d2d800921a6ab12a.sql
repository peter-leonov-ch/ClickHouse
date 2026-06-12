WITH cte AS
    (
        SELECT id2
        FROM tbl
        WHERE joinGet(currentDatabase() || '.join_engine', 'v', id1, id2) = tbl.v
    )
SELECT uniq(id2) AS count
FROM
(
    
    
    
    
    SELECT *
    FROM tbl AS e
    WHERE joinGet(currentDatabase() || '.join_engine', 'v', id1, id2) = e.v
)
WHERE id2 IN (
    SELECT id2
    FROM cte
)
UNION ALL
SELECT uniq(id2) AS count
FROM cte
