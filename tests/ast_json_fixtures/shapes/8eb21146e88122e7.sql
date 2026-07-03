WITH RECURSIVE recursive_cte AS (SELECT materialize(toUInt8(1)) AS n UNION ALL SELECT materialize(toUInt8(n + 1)) FROM recursive_cte WHERE n < 10)
SELECT n FROM recursive_cte FORMAT Null SETTINGS max_recursive_cte_evaluation_depth = 5
