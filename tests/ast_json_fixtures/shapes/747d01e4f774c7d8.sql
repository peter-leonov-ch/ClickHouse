create table t1(a Array(UInt32)) ENGINE = MergeTree ORDER BY tuple() as select [1,2]
