SELECT ClientIP AS x, x - 1, x - 2, x - 3, count() AS c FROM test.hits_s3 GROUP BY x, x - 1, x - 2, x - 3 ORDER BY c DESC LIMIT 10
