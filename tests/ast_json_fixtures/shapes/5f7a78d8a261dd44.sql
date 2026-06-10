create table xp_d as xp Engine=Distributed(test_shard_localhost, currentDatabase(), xp)
