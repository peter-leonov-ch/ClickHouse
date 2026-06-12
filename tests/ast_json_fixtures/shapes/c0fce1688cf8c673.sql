create table if not exists sample_prewhere (date Date, id Int32, time Int64) engine = MergeTree partition by date order by (id, time, intHash64(time)) sample by intHash64(time)
