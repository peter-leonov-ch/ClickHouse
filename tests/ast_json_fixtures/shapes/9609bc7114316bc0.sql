SELECT _shard_num, key, b.host_name, b.host_address IN ('::1', '127.0.0.1'), b.port
FROM dist_1 a
JOIN system.clusters b
ON _shard_num = b.shard_num
WHERE b.cluster = 'test_cluster_two_shards_localhost'
ORDER BY key
SETTINGS enable_analyzer = 1
