WITH (
        SELECT max(i)
        FROM t1
    ) AS value
SELECT
    value AS i,
    value AS j,
    value AS k,
    value AS l
FROM t1
