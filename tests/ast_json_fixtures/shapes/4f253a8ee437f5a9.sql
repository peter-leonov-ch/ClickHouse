select sum(y) from file(currentDatabase() || '_03914.parquet') prewhere x settings optimize_move_to_prewhere = 0
