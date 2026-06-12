SELECT * FROM x WHERE (D AND 5) OR ((C AND E) AND (C AND E)) ORDER BY ALL LIMIT 3 SETTINGS optimize_extract_common_expressions = 0
