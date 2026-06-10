WITH (
        SELECT dummy AS x
        FROM system.one
    ) AS y
SELECT
    y,
    min(dummy)
FROM remote('127.0.0.{1,2}', system.one)
GROUP BY y
