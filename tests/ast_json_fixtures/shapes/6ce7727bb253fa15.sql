create table x(i int, index mm LOG2(i) type minmax granularity 1, projection p (select MAX(i))) engine ReplicatedMergeTree('/clickhouse/tables/{database}/x', 'r') order by i
