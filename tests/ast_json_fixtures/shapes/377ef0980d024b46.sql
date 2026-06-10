SELECT v1, v2 FROM tab1 WHERE v2 > 100000 ORDER BY v1 ASC LIMIT 10 SETTINGS use_top_k_dynamic_filtering=1
