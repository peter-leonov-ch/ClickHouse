create table data_r2 engine=ReplicatedMergeTree('/tables/{database}', 'r2') order by tuple()
