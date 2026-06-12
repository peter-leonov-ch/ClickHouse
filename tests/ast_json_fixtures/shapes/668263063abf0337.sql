SELECT key, sum(agg) FROM tbl GROUP BY key WITH totals ORDER BY key SETTINGS use_query_cache = 1
