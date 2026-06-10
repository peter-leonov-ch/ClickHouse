WITH CAST([0.0, 2.0] AS Array(Float64)) AS reference_vec
SELECT id, L2Distance(vec, reference_vec)
FROM tab_f32
ORDER BY L2Distance(vec, reference_vec)
LIMIT 4
