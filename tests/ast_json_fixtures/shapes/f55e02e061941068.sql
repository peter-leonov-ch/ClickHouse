SELECT arraySplit(x -> 0, []) WHERE materialize(1) GROUP BY (0, ignore('a')) WITH ROLLUP SETTINGS group_by_use_nulls = 1
