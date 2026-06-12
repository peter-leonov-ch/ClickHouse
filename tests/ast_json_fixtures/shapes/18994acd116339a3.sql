CREATE TABLE alias10 AS alias_local10 ENGINE = Distributed(test_shard_localhost, currentDatabase(), alias_local10, cityHash64(Id))
