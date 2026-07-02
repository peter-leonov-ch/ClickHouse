SELECT grouping(key_a) + grouping(key_b) AS d
FROM
    (
        SELECT
            rand() % 10 AS key_a,
            rand(toLowCardinality(1)) % 5 AS key_b,
            number
        FROM numbers(100)
        )
GROUP BY
    key_a,
    key_b
WITH ROLLUP
FORMAT Null
