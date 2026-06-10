CREATE TABLE ORDERS AS ORDERS_shard
ENGINE = Distributed('test_shard_localhost', currentDatabase(), ORDERS_shard, rand())
