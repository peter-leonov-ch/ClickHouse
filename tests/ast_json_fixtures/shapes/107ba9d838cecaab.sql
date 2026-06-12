CREATE TABLE IF NOT EXISTS test_move_partition_src (
    pk UInt8,
    val UInt32
) Engine = MergeTree()
  PARTITION BY pk
  ORDER BY (pk, val) SETTINGS index_granularity = 8192, index_granularity_bytes = '10Mi'
