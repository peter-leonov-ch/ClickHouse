create table d (i int, j int) engine MergeTree partition by i % 2 order by tuple() settings index_granularity = 1
