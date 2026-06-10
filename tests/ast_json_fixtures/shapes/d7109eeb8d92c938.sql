WITH t2 AS
    (
        SELECT
            'x' AS s,
            number
        FROM numbers_mt(10000.)
    )
SELECT t1.s
FROM t AS t1
INNER JOIN t2 ON substr(t1.s, 1, 1) = t2.s
LIMIT 1e5
SETTINGS max_threads = 32, max_memory_usage = '2Gi', join_algorithm = 'parallel_hash', min_joined_block_size_bytes = '1Mi'
FORMAT Null
