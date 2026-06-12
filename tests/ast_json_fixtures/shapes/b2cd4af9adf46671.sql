SELECT key, sum(val) FROM (SELECT number AS key, number as val FROM numbers(2_000_000)) GROUP BY key
SETTINGS log_comment='03772_temporary_files_codec/agg', temporary_files_codec = 'LZ4'
FORMAT Null
