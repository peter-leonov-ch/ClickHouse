create table if not exists right_table engine=Distributed('test_shard_localhost', currentDatabase(), right_table_local) AS right_table_local
