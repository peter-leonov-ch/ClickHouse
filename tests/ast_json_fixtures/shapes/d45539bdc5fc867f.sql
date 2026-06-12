SELECT * FROM x INNER JOIN y ON ((x.A = y.A ) AND x.B = 1) OR ((x.A = y.A) AND y.C = 1) ORDER BY ALL LIMIT 10 SETTINGS optimize_extract_common_expressions = 0
