create table b (i int) engine MergeTree order by tuple() settings index_granularity = 2
