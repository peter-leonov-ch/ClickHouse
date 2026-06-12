CREATE DICTIONARY hierarchical_dictionary
(
    id UInt64,
    parent_id UInt64 HIERARCHICAL,
    name String
)
PRIMARY KEY id
SOURCE(CLICKHOUSE(TABLE 'hierarchy_source'))
LAYOUT(HASHED())
LIFETIME(0)
