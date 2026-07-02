SELECT
    key_a + key_b AS d,
    rank() OVER () AS f
FROM
    (
        SELECT
            rand() % 10 AS key_a,
            rand(1) % 5 AS key_b,
            number
        FROM numbers(100)
        )
GROUP BY
    key_a,
    key_b
WITH ROLLUP
ORDER BY multiIf(d = 0, key_a, NULL) ASC
FORMAT Null
