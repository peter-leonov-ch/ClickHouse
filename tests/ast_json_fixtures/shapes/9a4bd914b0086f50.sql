WITH
    a as key
SELECT
    a as k1,
    sum(b) as k2
FROM
    test
GROUP BY
    key
ORDER BY k1, k2
