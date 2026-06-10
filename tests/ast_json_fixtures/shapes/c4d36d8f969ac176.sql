CREATE TABLE test (x UInt8, y UInt8, z String DEFAULT toString(x)) PARTITION BY x ORDER BY x
