select *, _part_offset + _part_starting_offset from test where _part_offset + _part_starting_offset = 8 settings parallel_replicas_local_plan = 0, max_rows_to_read = 1
