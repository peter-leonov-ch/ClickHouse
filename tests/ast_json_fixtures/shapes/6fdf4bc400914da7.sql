create table x (i int, j int) engine MergeTree partition by i order by j settings index_granularity = 1
