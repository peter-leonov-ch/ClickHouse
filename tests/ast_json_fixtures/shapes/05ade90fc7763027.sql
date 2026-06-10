CREATE TABLE 01686_test (key UInt64, value String) Engine=EmbeddedRocksDB PRIMARY KEY(key) SETTINGS optimize_for_bulk_insert = 0
