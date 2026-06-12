create table dist_01527 as data_01527 engine=Distributed('test_cluster_two_shards', currentDatabase(), data_01527, dictGetUInt64('db_01527_ranges.dict', 'shard', key))
