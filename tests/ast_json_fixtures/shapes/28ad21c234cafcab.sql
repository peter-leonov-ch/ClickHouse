SELECT * FROM remote('127.0.0.{1,2}', numbers(40), number) ORDER BY 'a' LIMIT 1 BY number SETTINGS optimize_skip_unused_shards = 1, force_optimize_skip_unused_shards=0 format Null
