SELECT k, key, toTypeName(value2), value2 FROM t2 LEFT JOIN rdb ON rdb.key == t2.k ORDER BY k SETTINGS join_use_nulls = 1
