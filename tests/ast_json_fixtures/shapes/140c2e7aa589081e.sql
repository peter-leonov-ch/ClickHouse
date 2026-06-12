WITH (60 * 60) * 24 AS d
select toStartOfDay(x) as k, sum(y) as v,
  (z + d) * (z + d - 1) / 2 - (toUInt64(k - toDateTime('2000-01-01', 'UTC')) as z) * (z - 1) / 2 as est,
  est - v as delta
from tab final group by k order by k
settings max_threads=8, optimize_aggregation_in_order=1, split_parts_ranges_into_intersecting_and_non_intersecting_final=1
