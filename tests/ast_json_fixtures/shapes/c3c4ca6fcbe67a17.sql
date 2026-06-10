CREATE TABLE map_test(`tags` Map(String, String)) ENGINE = MergeTree PRIMARY KEY tags ORDER BY tags SETTINGS index_granularity = 8192
