select count(), * from dist_01247 group by number order by count(), number offset 1 settings distributed_push_down_limit=1
