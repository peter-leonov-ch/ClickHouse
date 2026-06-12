CREATE TABLE merge_00401 AS stripe1 ENGINE = Merge(currentDatabase(), '^stripe\\d+')
