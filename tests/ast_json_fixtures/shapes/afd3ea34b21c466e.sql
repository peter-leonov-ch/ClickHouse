SELECT
    x,
    max(A) OVER (PARTITION BY x % 1000) AS mA,
    max(B) OVER (PARTITION BY x % 1000) AS mB,
    max(C) OVER (PARTITION BY x % 1000) AS mC
FROM x
QUALIFY (mA AND mB) OR (mA AND mC)
ORDER BY x
LIMIT 10
SETTINGS optimize_extract_common_expressions = 0
