CREATE TABLE merge_table Engine=Merge(currentDatabase(), '^(table_to_merge_[a-z])$') AS table_to_merge_a
