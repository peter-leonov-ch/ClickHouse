INSERT INTO FUNCTION remote('localhost', currentDatabase(), tab) SELECT * FROM numbers(1) LIMIT 1
