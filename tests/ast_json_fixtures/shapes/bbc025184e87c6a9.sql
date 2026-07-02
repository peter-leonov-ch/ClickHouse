SELECT REGEXP_REPLACE(trimLeft(explain), '__set_Int32_\\d+_\\d+', '__set_Int32_UNIQ_ID')
FROM (
        EXPLAIN actions=1
        SELECT t1.k, t1.a, t2.x
        FROM tp1 AS t1
        JOIN tp2 AS t2 ON t1.k = t2.k
        WHERE (t2.x = 100) OR (t2.x = 200)
        ORDER BY t1.k
    )

WHERE explain ILIKE '%Filter column: %' SETTINGS enable_parallel_replicas = 0
FORMAT TSV
