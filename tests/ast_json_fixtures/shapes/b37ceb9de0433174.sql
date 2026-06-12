select count() from remote('127.{1,2}', currentDatabase(), data_02176) where key = 0 group by key settings distributed_aggregation_memory_efficient=0
