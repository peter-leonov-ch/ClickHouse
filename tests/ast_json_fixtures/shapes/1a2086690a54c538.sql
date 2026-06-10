SELECT * FROM (SELECT any(_shard_num) shard_num, count(), uniq(dummy) FROM remote('127.0.0.{2,3}', system.one)) ORDER BY shard_num LIMIT 1, 1 SETTINGS distributed_group_by_no_merge=2
