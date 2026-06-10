CREATE TABLE Test
ENGINE = MergeTree()
PRIMARY KEY (String1,String2)
ORDER BY (String1,String2)
SETTINGS index_granularity = 8192, index_granularity_bytes = '10Mi', add_minmax_index_for_numeric_columns=0
AS
SELECT
   'String1_' || toString(number) as String1,
   'String2_' || toString(number) as String2,
   'String3_' || toString(number) as String3,
   'String4_' || toString(number%4) as String4
FROM numbers(1)
