INSERT INTO FUNCTION cluster('test_shard_localhost', currentDatabase(), x, rand()) SELECT * FROM numbers(10)
