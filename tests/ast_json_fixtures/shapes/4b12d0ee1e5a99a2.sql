create table test_table as test_table_sharded
engine=Distributed(test_cluster_two_shards, currentDatabase(), test_table_sharded, hash)
