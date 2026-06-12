SELECT tuple(tuple(number)) AS x FROM numbers(10) WHERE toString(toUUID(tuple(number), NULL), x) GROUP BY number, (toString(x), number) WITH CUBE SETTINGS group_by_use_nulls = 1 FORMAT Null
