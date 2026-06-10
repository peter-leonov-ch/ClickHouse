CREATE DICTIONARY memory_db.dict2
(
  key_column UInt64 DEFAULT 0 INJECTIVE,
  second_column UInt8 DEFAULT 1 EXPRESSION rand() % 222,
  third_column String DEFAULT 'qqq'
)
PRIMARY KEY key_column
SOURCE(CLICKHOUSE(HOST 'localhost' PORT tcpPort() USER 'default' TABLE 'table_for_dict' PASSWORD '' DB 'database_for_dict_01018'))
LIFETIME(MIN 1 MAX 10)
LAYOUT(FLAT())
