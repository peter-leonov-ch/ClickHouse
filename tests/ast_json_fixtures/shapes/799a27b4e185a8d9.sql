SELECT id, count() AS c
FROM test_limit_by_all
GROUP BY id, category
HAVING c >= 1
ORDER BY id, c DESC, category
LIMIT 1 BY ALL
