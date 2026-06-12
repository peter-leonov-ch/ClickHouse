SELECT x, max(A) AS mA, max(B) AS mB, max(C) AS mC FROM x GROUP BY x HAVING (mA AND mB) OR (mA AND mC) ORDER BY x LIMIT 10 SETTINGS optimize_extract_common_expressions = 0
