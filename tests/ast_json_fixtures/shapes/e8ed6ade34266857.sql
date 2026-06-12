SELECT 'both', database, table, left(replica_name, 2) FROM system.replicas WHERE database = currentDatabase()
