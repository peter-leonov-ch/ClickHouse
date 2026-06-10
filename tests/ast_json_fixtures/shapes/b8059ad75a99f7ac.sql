create table test (d Dynamic) engine=MergeTree order by tuple() partition by d
