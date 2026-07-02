SELECT * FROM tab
WHERE u64_countmin > 3500 and u64_countmin < 3600
FORMAT NULL
SETTINGS use_statistics_cache = 0, log_comment = '03904_empty'
