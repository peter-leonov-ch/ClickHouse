SELECT (2,) = tuple(materialize(1)) GROUP BY 1 WITH ROLLUP SETTINGS group_by_use_nulls = 1, enable_positional_arguments = 1
