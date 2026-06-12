SELECT *
FROM ( SELECT * FROM test_grouping_sets_predicate GROUP BY GROUPING SETS ( (day_, type_1), (day_) ) )
WHERE day_ = '2023-01-05'
GROUP BY *
ORDER BY ALL
SETTINGS enable_analyzer=1
