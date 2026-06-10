SELECT a, e
FROM test_lazy_read_in_order PREWHERE e > 100
ORDER BY a
LIMIT 5
