SELECT u.uid, u.name, u.gid, u.gname
FROM users u left join groups g using gid, gname
format PrettyCompactMonoBlock
