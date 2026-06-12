select c1, count(*) from group_by_null_key group by ROLLUP(c1)
