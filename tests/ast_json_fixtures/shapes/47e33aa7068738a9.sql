ALTER TABLE t_explicit_ops ADD COLUMN c UInt64 DEFAULT 0, ADD INDEX idx_explicit_a a TYPE minmax GRANULARITY 1
