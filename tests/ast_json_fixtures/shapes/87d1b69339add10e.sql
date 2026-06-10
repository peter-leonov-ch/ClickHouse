WITH test1 AS (SELECT n FROM with_test order by n limit 100)
SELECT max(n) FROM test1 where n=42
