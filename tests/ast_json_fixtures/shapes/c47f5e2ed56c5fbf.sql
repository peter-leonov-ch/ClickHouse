create table t (i int, j int, projection x (select * order by j)) engine MergeTree partition by i order by i
