CREATE TABLE dist (key int, INDEX i1 key TYPE minmax GRANULARITY 1) Engine=Distributed(test_shard_localhost, currentDatabase(), 'foo')
