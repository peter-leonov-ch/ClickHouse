WITH test1 AS (SELECT n, null b, n+1 m FROM with_test where  n = 42 order by n limit 4)
SELECT max(n) m FROM test1 where b is null and test1.m=43 having m=42 limit 4
