SELECT b + 1 AS a, * FROM tb JOIN tabc USING (a) ORDER BY ALL SETTINGS enable_analyzer = 1
