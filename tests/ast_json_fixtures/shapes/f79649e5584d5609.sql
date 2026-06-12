SELECT 1 AS x FROM remote('127.0.0.{2,3}', system.one) ORDER BY dummy LIMIT 1 BY x
