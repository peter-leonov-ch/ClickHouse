SELECT 'distributed_0', _shard_num, toUInt8(1) AS dummy FROM d_one AS o WHERE o.dummy = 0 ORDER BY _shard_num
