CREATE TABLE test_merge as test_local
ENGINE = Merge(currentDatabase(), 'test_local')
