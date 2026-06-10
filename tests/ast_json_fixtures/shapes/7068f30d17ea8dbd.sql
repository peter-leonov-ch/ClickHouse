WITH RECURSIVE recursive_cte AS (SELECT 1 AS n UNION ALL SELECT n + 1 FROM recursive_cte)
SELECT n FROM recursive_cte LIMIT 5
