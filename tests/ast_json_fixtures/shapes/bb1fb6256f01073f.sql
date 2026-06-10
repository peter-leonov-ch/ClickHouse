SELECT number FROM (SELECT number FROM numbers(2097152)) ORDER BY number * 1234567890123456789 LIMIT 2097142, 10
SETTINGS log_comment='02402_external_disk_mertrics/sort'
FORMAT Null
