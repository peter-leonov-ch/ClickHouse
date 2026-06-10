CREATE TABLE check_system_tables
  (
    name1 UInt8,
    name2 UInt8,
    name3 UInt8
  ) ENGINE = MergeTree()
    ORDER BY name1
    PARTITION BY name2
    SAMPLE BY name1
    SETTINGS min_bytes_for_wide_part = 0, compress_marks = false, compress_primary_key = false, ratio_of_defaults_for_sparse_serialization = 1
