SELECT
    database,
    table,
    name,
    type,
    type_full,
    granularity,
    data_compressed_bytes > 100,
    data_uncompressed_bytes > 75,
    marks_bytes
FROM system.data_skipping_indices WHERE database = currentDatabase() AND type = 'text' FORMAT Vertical
