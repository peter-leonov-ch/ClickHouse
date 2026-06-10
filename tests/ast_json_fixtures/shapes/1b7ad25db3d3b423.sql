create table t_l5ydey on cluster test_shard_localhost as local_t_l5ydey
    engine=Distributed('test_shard_localhost', currentDatabase(),'local_t_l5ydey', rand())
