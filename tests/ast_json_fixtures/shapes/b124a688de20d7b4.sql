create table data_01801 (key Int) engine=MergeTree() order by key settings index_granularity=10 as select number/10 from numbers(100)
