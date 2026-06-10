SELECT
    k,
    count()
FROM t1
WHERE k > 0
GROUP BY k
ORDER BY k
SETTINGS force_primary_key = 1
