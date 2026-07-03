SELECT * FROM (SELECT number, 'payload' FROM numbers(2_000_000)) ORDER BY number
SETTINGS log_comment='03772_temporary_files_codec/sort', temporary_files_codec = 'LZ4'
FORMAT Null
