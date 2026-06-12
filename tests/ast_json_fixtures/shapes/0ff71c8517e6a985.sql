select * from remote('127.{2..11}', view(select * from numbers(1e6))) group by number order by number limit 20 settings distributed_group_by_no_merge=0, max_memory_usage='100Mi'
