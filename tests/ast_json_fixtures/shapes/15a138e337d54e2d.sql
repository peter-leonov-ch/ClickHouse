CREATE TABLE IF NOT EXISTS test_sequenceNextNode (dt DateTime, id int, action String) ENGINE = MergeTree() PARTITION BY dt ORDER BY id
