create table if not exists t_group_by_lowcardinality(p_date Date, val LowCardinality(Nullable(String))) 
engine=MergeTree() partition by p_date order by tuple()
