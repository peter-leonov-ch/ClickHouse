CREATE TABLE recompression_table_compact
(
  dt DateTime,
  key UInt64,
  value String

) ENGINE MergeTree()
ORDER BY tuple()
PARTITION BY key
TTL dt + INTERVAL 1 MONTH RECOMPRESS CODEC(ZSTD(17)), dt + INTERVAL 1 YEAR RECOMPRESS CODEC(LZ4HC(10))
SETTINGS min_rows_for_wide_part = 10000, min_bytes_for_full_part_storage = 0
