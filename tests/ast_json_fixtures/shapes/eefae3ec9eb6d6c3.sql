WITH RECURSIVE
    recursive AS (
            SELECT id FROM tab WHERE id = 'uuid3'
        UNION ALL
            SELECT parent AS id
            FROM tab
            WHERE tab.id IN recursive AND parent != 'empty'
            GROUP BY parent
    )
SELECT *
FROM recursive
GROUP BY id
ORDER BY id
