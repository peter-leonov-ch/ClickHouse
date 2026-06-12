create table nums_in_mem_dist as nums_in_mem engine=Distributed('test_shard_localhost', currentDatabase(), nums_in_mem)
