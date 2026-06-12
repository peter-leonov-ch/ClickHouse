CREATE TABLE IF NOT EXISTS 03578_rocksdb
(
    key UInt16,
    val String
)
ENGINE = EmbeddedRocksDB()
PRIMARY KEY key
