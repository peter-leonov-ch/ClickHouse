create table t (i int, j int, k int, projection p (select * order by j)) engine MergeTree order by i settings index_granularity = 1
