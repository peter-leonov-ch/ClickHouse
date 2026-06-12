CREATE TABLE dist_test_01040 AS test_01040 Engine=Distributed(test_cluster_two_shards, currentDatabase(), test_01040, key) SETTINGS
    background_insert_batch=1,
    background_insert_sleep_time_ms=10,
    background_insert_max_sleep_time_ms=100
