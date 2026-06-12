INSERT INTO FUNCTION remote('127.0.0.{1,2}', currentDatabase(), y, 'default', rand()) SELECT * FROM numbers(10)
