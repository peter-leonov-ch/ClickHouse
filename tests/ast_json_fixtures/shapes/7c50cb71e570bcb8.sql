INSERT INTO TABLE FUNCTION clusterAllReplicas('test_shard_localhost', currentDatabase(), 'distributed_01099_c') (n) SELECT number FROM remote('localhost', numbers(5)) tx
