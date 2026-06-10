CREATE TABLE t0 (c0 Int, INDEX i0 (c0, c0) TYPE hypothesis) ENGINE = MergeTree() ORDER BY tuple() SETTINGS allow_suspicious_indices = 1
