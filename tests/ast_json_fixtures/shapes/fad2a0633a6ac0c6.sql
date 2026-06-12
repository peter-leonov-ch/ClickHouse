SELECT 0 AS `0`, 18 AS `18`, __table1.k AS k
FROM test__fuzz_2_dist AS __table1
PREWHERE _CAST(11, 'Nullable(UInt8)')
ORDER BY __table1.k DESC NULLS LAST
LIMIT 0.9999, _CAST(100, 'UInt64')
