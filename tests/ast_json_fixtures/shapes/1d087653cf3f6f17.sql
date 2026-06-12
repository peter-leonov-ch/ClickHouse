CREATE DICTIONARY IF NOT EXISTS dictdb_01376.dict_exists
(
  key_column UInt64,
  value Float64 DEFAULT 77.77 INJECTIVE
)
PRIMARY KEY key_column
SOURCE(CLICKHOUSE(HOST 'localhost' PORT tcpPort() USER 'default' TABLE 'table_for_dict' DB 'dictdb_01376'))
LIFETIME(1)
LAYOUT(FLAT())
