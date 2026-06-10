WITH [0.0, 2.0] AS reference_vec
SELECT id, L2Distance(vec, reference_vec)
FROM tab_bf16
ORDER BY L2Distance(vec, reference_vec)
LIMIT 4
