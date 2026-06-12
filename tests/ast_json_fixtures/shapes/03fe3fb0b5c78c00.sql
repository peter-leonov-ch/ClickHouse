create table mt (n UInt64) engine=MergeTree order by n partition by n % 10
