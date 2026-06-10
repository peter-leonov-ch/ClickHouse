SELECT ignore(x), count() FROM (SELECT number AS x FROM system.numbers LIMIT 1000 UNION ALL SELECT number AS x FROM system.numbers LIMIT 1000) GROUP BY x WITH TOTALS LIMIT 10 FORMAT JSONCompact
