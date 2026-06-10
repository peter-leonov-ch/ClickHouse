create table xy(x int, y int) engine MergeTree partition by intHash64(x) % 2 order by y settings index_granularity = 1
