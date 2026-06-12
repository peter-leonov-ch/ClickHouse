CREATE DICTIONARY hierarchy_flat_dictionary
(
    id UInt64,
    parent_id UInt64 HIERARCHICAL
)
PRIMARY KEY id
SOURCE(CLICKHOUSE(TABLE 'hierarchy_source_table'))
LAYOUT(FLAT())
LIFETIME(MIN 1 MAX 1000)
