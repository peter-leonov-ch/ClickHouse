alter table x add index nn LOG2(i) type minmax granularity 1, add projection p2 (select MIN(i))
