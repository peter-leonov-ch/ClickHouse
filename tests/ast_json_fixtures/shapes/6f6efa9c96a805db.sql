INSERT INTO FUNCTION remote('127.0.0.{1,2}', currentDatabase(), y, 'default') SELECT * FROM numbers(10)
