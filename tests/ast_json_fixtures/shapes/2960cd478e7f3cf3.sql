SELECT DISTINCT throwIf(empty(partition)) FROM system.part_log WHERE database = currentDatabase()
