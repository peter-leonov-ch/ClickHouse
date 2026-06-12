create table alter_ttl(i Int) engine = MergeTree order by i ttl toDate('2020-05-05')
