SELECT 3 + 3 from numbers(10) GROUP BY GROUPING SETS (('str'), (3 + 3)) order by all
