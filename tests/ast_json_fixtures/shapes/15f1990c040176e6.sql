CREATE TABLE check_system_tables AS check_system_tables_null Engine=Distributed(test_shard_localhost, currentDatabase(), check_system_tables_null)
