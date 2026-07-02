SELECT a FROM ( SELECT key + 1 as a, key FROM t1 GROUP BY key ) WHERE key FORMAT Null
