CREATE DICTIONARY direct_dictionary
(
    id UInt64,
    v1 String,
    v2 Nullable(String) DEFAULT NULL,
    v3 Nullable(UInt64)
)
PRIMARY KEY id
SOURCE(CLICKHOUSE(TABLE 'dictionary_source_table'))
LAYOUT(DIRECT())
