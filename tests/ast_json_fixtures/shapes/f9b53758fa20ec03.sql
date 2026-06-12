create table dist_out as data engine=Distributed(test_shard_localhost, currentDatabase(), data)
