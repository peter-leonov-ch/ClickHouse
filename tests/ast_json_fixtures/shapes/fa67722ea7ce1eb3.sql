WITH [toFloat32(0), 1, 2] AS reference_vec SELECT id, round(L2DistanceTransposed(vec.1, vec.2, vec.3, vec.4, 0, reference_vec), 5) AS dist FROM qbit ORDER BY id
