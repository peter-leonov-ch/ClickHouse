SELECT
    key,
    ref_valueD, valueD, ref_valueD - valueD as dD
FROM codecTest
WHERE
    dD != 0
LIMIT 10
