SELECT count(arraySplit(x -> toUInt8(number), [])) FROM numbers(10) GROUP BY number, [number] WITH ROLLUP settings group_by_use_nulls=1
