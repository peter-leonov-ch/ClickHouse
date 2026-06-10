CREATE TABLE test_structure (
    t Nullable(Tuple(x UInt32, y UInt64)),
    PRIMARY KEY ()
) ENGINE = MergeTree
SETTINGS ratio_of_defaults_for_sparse_serialization = 0, nullable_serialization_version = 'allow_sparse', min_bytes_for_wide_part = 0
