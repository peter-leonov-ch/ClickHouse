BACKUP TABLE db.t AS db.t2 PARTITIONS 'p1', 'p2' TO Disk('d', 'path') SETTINGS async = 1 FORMAT Null
