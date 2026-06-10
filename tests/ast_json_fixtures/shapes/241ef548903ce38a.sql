CREATE TABLE `01746_dist` AS `01746_local`
ENGINE = Distributed('test_shard_localhost', currentDatabase(), `01746_local`, rand())
