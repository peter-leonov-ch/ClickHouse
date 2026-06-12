create table t_light(a int, b int, c int, index i_c(b) type minmax granularity 4) engine = MergeTree order by a partition by c % 5 settings min_bytes_for_wide_part=0
