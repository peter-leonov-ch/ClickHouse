WITH test1 AS (SELECT n, null b, n+1 m FROM with_test where 1=0 or n = 42 order by n limit 4)
SELECT max(n) m FROM test1 where test1.m=43 having max(n)=42
