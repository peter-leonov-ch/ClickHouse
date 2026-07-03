SELECT
    table,
    type,
    new_part_name,
    num_postponed > 0 AS has_postpones,
    postpone_reason
FROM system.replication_queue
WHERE table LIKE 'execute\\_on\\_single\\_replica\\_r%'
AND database = currentDatabase()
ORDER BY table
FORMAT Vertical
