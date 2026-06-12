SELECT id, finalizeAggregation(v) AS vv FROM t_index_agg_func FINAL WHERE vv >= 10 ORDER BY id
