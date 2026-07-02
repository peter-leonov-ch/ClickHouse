SELECT * FROM t ALL RIGHT JOIN tj ON t.key1 == tj.key1 AND t.key2 == tj.key2 AND 1 ORDER BY ALL SETTINGS query_plan_use_new_logical_join_step = 0 FORMAT TSVWithNames
