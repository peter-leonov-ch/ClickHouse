SELECT number * 246 + 10 AS n, toDate('2000-01-01') + n AS d, (n, d) AS tuple FROM system.numbers LIMIT 1 FORMAT TabSeparatedWithNamesAndTypes SETTINGS enable_analyzer=1
