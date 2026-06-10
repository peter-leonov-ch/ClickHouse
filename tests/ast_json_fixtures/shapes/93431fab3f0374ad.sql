CREATE DICTIONARY db_01268.dict2
(
  region_id UInt64 DEFAULT 0,
  parent_region UInt64 DEFAULT 0 HIERARCHICAL,
  region_name String DEFAULT ''
)
PRIMARY KEY region_id
SOURCE(CLICKHOUSE(HOST 'localhost' PORT tcpPort() USER 'default' TABLE 'table_for_dict2' PASSWORD '' DB 'database_for_dict_01268'))
LAYOUT(DIRECT()) SETTINGS(dictionary_use_async_executor=1, max_threads=8)
