SELECT n, j * 2097152 FROM
(SELECT number * 200000 as n FROM numbers(5)) nums
ANY LEFT JOIN ( SELECT number * 2 AS n, number AS j FROM numbers(1000000) ) js2
USING n
ORDER BY n
SETTINGS log_comment='02402_external_disk_mertrics/grace_join'
FORMAT Null
