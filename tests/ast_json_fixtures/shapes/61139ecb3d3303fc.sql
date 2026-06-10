create table t (s UInt16, l UInt16, projection p (select s, l order by l)) engine MergeTree order by s
