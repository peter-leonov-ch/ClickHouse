SELECT total_rows, total_bytes > 0 FROM system.tables WHERE database = currentDatabase() AND name = 'dict' FORMAT CSV
