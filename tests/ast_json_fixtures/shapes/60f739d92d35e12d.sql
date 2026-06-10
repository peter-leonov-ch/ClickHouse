SELECT number FROM remote('127.0.0.{2,3}', currentDatabase(), data_00184) LIMIT 1 BY number LIMIT 1 SETTINGS distributed_group_by_no_merge=2
