CREATE MATERIALIZED VIEW m02006 on cluster test_shard_localhost
Engine = MergeTree ORDER BY tuple() AS SELECT d, 0 AS i FROM t02006 GROUP BY d, i
format Null
