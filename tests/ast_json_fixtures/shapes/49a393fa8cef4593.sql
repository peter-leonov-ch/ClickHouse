create table x (dt DateTime, i Int32) engine MergeTree partition by indexHint(dt) order by dt TTL dt + toIntervalDay(15) settings index_granularity = 8192
