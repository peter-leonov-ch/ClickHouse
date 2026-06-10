select *, (select toDateTime64(0, 3)) from remote('127.0.0.1', system.one) settings prefer_localhost_replica=0
