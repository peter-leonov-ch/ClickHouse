SELECT name, toUInt32(metadata_modification_time) > 0, engine_full, create_table_query FROM system.tables WHERE database = currentDatabase() ORDER BY name FORMAT TSVRaw
