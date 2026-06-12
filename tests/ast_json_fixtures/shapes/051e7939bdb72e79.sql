SELECT tuple(number) AS x FROM numbers(10) GROUP BY GROUPING SETS (number) order by x
