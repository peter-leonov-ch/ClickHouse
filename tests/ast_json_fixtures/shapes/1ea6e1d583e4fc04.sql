CREATE DICTIONARY hierarchy_flat_dictionary_index
(
    id UInt64,
    parent_id UInt64 BIDIRECTIONAL
)
PRIMARY KEY id
SOURCE(CLICKHOUSE(TABLE 'test_hierarchy_source_table'))
LAYOUT(FLAT())
LIFETIME(0)
