CREATE TABLE visits_order
(
    user_id UInt64,
    user_name String,
    some_int UInt64
) ENGINE = MergeTree() PRIMARY KEY user_id PARTITION BY user_id SETTINGS index_granularity = 1
