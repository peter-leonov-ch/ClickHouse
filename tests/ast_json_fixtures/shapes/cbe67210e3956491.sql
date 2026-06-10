CREATE OR REPLACE DICTIONARY test_01915_db.test_dictionary
(
    id UInt64,
    value String
)
PRIMARY KEY id
LAYOUT(DIRECT())
SOURCE(CLICKHOUSE(DB 'test_01915_db' TABLE 'test_source_table_1'))
