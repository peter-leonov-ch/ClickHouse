CREATE TABLE merged as short ENGINE = Merge(currentDatabase(), 'short|long')
