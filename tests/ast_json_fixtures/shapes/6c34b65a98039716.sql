SELECT arraySplit(x -> toUInt8(number), []) from numbers(1) GROUP BY toUInt8(number) WITH ROLLUP SETTINGS group_by_use_nulls = 1
