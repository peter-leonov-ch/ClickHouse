SELECT
    tup.u as u_value,
    count() as cnt,
    sum(n) as total_n,
    avg(n) as avg_n,
    groupArray(id) as ids
FROM tuple_test
WHERE tup IS NOT NULL
GROUP BY tup.u
HAVING cnt > 0 AND avg_n > 200
ORDER BY u_value
