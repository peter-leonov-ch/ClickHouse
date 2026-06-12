(select * from v join t1 using k order by all)
except
(select * from v join t1 using k order by all settings enable_parallel_replicas=0)
