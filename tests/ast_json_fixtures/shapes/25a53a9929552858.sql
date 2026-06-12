ATTACH TABLE primary_key_test(v1 Int32, v2 Int32, PRIMARY KEY(v1, v2)) ENGINE=ReplacingMergeTree ORDER BY (v1, v2)
