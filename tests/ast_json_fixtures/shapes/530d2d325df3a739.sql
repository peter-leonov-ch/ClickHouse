SELECT sum(a) FROM remote('127.0.0.4', currentDatabase(), '02863_delayed_source') GROUP BY a ORDER BY a LIMIT 1 FORMAT JSON settings output_format_write_statistics=0
