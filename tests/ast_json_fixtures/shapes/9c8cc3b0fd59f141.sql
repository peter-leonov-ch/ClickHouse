CREATE TABLE primary_key_test(v1 Int64, v2 Int32, v3 String, PRIMARY KEY(v1, gcd(v1, v2))) ENGINE=ReplacingMergeTree ORDER BY v1
