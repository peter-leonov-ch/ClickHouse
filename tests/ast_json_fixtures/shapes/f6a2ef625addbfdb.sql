EXECUTE AS test_03727 ALTER TABLE normal UPDATE s = (SELECT * FROM remote('localhost', currentDatabase(), 'secret') LIMIT 1) WHERE n=1
