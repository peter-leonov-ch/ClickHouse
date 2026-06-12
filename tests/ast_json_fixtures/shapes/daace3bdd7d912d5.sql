SELECT toUnixTimestamp(v1), v2 FROM tab1 ORDER BY v1 ASC LIMIT 5 SETTINGS use_top_k_dynamic_filtering=1
