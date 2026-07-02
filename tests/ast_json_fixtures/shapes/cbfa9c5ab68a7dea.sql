SELECT number % 2 ? [1, 2] : [3, 4, 5] AS res FROM system.numbers LIMIT 10 FORMAT TabSeparatedWithNamesAndTypes
