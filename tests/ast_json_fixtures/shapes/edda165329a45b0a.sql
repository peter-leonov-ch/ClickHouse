CREATE TABLE test_optimize_exception (date Date) PARTITION BY toYYYYMM(date) ORDER BY date
