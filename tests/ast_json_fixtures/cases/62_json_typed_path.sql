CREATE TABLE t (j JSON(a.b UInt32, SKIP x, SKIP REGEXP 'y.*', max_dynamic_paths = 8)) ENGINE = Memory
