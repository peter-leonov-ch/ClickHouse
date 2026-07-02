SELECT age FROM remote('127.0.0.{2,3}', currentDatabase(), users) GROUP BY age LIMIT 20 FORMAT JSON SETTINGS output_format_write_statistics=0
