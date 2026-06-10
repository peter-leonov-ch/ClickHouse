WITH test1 AS (SELECT number-1 as n FROM numbers(42)) 
SELECT max(n+1)+1 z FROM test1 join test1 x using n having z - 1 = (select min(n-1)+41 from test1) + 2
