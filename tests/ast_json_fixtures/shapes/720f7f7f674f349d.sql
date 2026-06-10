SELECT tuple(number + 1) AS x FROM numbers(10) GROUP BY number + 1, toString(x) WITH CUBE settings group_by_use_nulls=1 FORMAT Null
