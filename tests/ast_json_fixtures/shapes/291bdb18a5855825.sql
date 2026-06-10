CREATE TABLE LINEITEM AS LINEITEM_shard
ENGINE = Distributed('test_shard_localhost', currentDatabase(), LINEITEM_shard, rand())
