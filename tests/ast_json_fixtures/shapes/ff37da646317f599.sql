CREATE DICTIONARY 02183_range_dictionary
(
  key UInt64,
  start UInt64,
  end UInt64
)
PRIMARY KEY key
SOURCE(CLICKHOUSE(TABLE '02183_range_dictionary_source_table'))
LAYOUT(RANGE_HASHED())
RANGE(MIN start MAX end)
LIFETIME(0)
