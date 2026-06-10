WITH test1 AS (SELECT number-1 as n FROM numbers(42)) 
SELECT max(n+1)+1 z FROM test1
