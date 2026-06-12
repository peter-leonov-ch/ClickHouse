SELECT
    2147483647,
    count(pow(NULL, 1.0001))
FROM remote(test_cluster_two_shards, system, one)
GROUP BY
    makeDateTime64(NULL, NULL, pow(NULL, '257') - '-1', '0.2147483647', 257),
    makeDateTime64(pow(pow(NULL, '21474836.46') - '0.0000065535', 1048577), '922337203685477580.6', NULL, NULL, pow(NULL, 1.0001) - 65536, NULL)
WITH CUBE
    SETTINGS enable_analyzer = 1
