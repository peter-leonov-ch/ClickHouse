create table a (i int, j int, projection p (select * order by j)) engine MergeTree partition by i order by tuple() settings index_granularity = 1
