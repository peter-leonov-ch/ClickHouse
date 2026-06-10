SELECT 1 AS x, x, (SELECT 2 AS x, x) FROM remote('127.0.0.{2,3}', system.one) WHERE (3, 4) IN (SELECT 3 AS x, toUInt8(x + 1))
