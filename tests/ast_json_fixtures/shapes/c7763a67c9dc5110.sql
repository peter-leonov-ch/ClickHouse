select sum(y) from file(currentDatabase() || '_03914.parquet') prewhere x where y >= 50
