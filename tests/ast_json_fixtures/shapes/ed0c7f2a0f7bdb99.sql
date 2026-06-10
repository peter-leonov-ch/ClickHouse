select count(), 42 AS z from remote('127.0.0.{2,3}', system.one) group by z WITH TOTALS
