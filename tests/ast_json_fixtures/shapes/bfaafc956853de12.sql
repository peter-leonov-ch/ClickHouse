WITH [0.0, 2.0] AS reference_vec
SELECT id
FROM tab
WHERE id >= 0
ORDER BY L2Distance(vec, reference_vec)
LIMIT 5
SETTINGS vector_search_with_rescoring = 0
