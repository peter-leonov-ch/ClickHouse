create table t(a UInt32) engine=MergeTree order by tuple() partition by a % 16
