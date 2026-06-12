SELECT DISTINCT tupleElement(data, 2) as category
FROM test_nullable_tuples
WHERE tupleElement(data, 2) IS NOT NULL
ORDER BY category
