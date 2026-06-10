SELECT *
FROM ( SELECT * FROM test_grouping_sets_predicate GROUP BY GROUPING SETS ( (*), (day_) ) )
WHERE day_ = '2023-01-05'
GROUP BY GROUPING SETS (*)
ORDER BY type_1
SETTINGS enable_analyzer=1
