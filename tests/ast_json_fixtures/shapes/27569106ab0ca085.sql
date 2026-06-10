CREATE TABLE check_query_comment_column
  (
    first_column UInt8 COMMENT 'comment 1',
    second_column UInt8 COMMENT 'comment 2',
    third_column UInt8 COMMENT 'comment 3'
  ) ENGINE = MergeTree()
        ORDER BY first_column
        PARTITION BY second_column
        SAMPLE BY first_column
