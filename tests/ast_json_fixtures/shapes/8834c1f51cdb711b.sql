CREATE TABLE alias_bug_dist
AS alias_bug
ENGINE = Distributed('test_shard_localhost', currentDatabase(), 'alias_bug', rand())
