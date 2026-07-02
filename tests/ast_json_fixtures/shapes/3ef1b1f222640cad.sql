SELECT name, count() AS c FROM test_table GROUP BY name WITH TOTALS ORDER BY name FORMAT JSONCompactStringsEachRowWithNamesAndTypes
