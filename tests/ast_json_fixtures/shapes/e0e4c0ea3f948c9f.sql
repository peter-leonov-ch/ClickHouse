INSERT INTO FUNCTION file((SELECT name FROM random_filename LIMIT 1), 'MsgPack', 'c0 Int32, c1 Tuple()')
SELECT 1, tuple() FROM numbers(5) SETTINGS engine_file_truncate_on_insert = 1
