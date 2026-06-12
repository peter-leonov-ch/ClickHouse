CREATE TABLE primary_key_test(v Int32) ENGINE=ReplacingMergeTree ORDER BY v PRIMARY KEY(v)
