SELECT '#02136_scalar_subquery_4', (SELECT max(number) FROM numbers(1000)) as n FROM system.numbers LIMIT 2
