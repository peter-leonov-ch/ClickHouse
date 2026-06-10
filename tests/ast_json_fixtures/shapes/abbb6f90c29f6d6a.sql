WITH [0.5, 0.5] AS reference_vec
SELECT id, vec, L2Distance(vec, reference_vec)
FROM tab
ORDER BY L2Distance(vec, reference_vec)
LIMIT 3
SETTINGS hnsw_candidate_list_size_for_search = 0
