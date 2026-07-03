SELECT count(*) FROM tab WHERE b = rand64() SETTINGS use_query_condition_cache = true FORMAT Null
