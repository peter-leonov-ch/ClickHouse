create table log_proxy_02572 as system.query_log engine=Distributed('test_shard_localhost', currentDatabase(), 'receiver_02572')
