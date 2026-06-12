select a, any(b), c, d from tab where b = 1 group by a, c, d order by c, d settings optimize_aggregation_in_order=1, query_plan_aggregation_in_order=1
