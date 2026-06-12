SELECT uniq(number) u FROM remote('127.0.0.{2,3}', currentDatabase(), data_00184) GROUP BY number ORDER BY u DESC SETTINGS distributed_group_by_no_merge=2
