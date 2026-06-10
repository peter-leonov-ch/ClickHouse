SELECT arrayJoin([(k1, v), (k2, v)]) AS row, row.1 as k FROM t FINAL WHERE k1 != 3 AND k = 1 ORDER BY row SETTINGS enable_vertical_final = 0
