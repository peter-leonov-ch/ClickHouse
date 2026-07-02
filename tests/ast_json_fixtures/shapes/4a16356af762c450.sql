WITH RECURSIVE x AS (SELECT 1 AS n UNION ALL SELECT sum(n) FROM x)
  SELECT * FROM x FORMAT NULL SETTINGS max_recursive_cte_evaluation_depth = 5
