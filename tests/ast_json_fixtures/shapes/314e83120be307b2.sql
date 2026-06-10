WITH [1., 2.] AS reference_vec
SELECT *
FROM tab
PREWHERE id < 5000
ORDER BY cosineDistance(vec, reference_vec) ASC
LIMIT 10
FORMAT Null
