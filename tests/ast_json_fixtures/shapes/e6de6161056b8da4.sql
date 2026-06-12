WITH map('foo', (1, 2), 'bar', (3, 4))::Map(String, Tuple(a UInt64, b UInt64)) AS m
SELECT m.keys, m.values, m.values.*
