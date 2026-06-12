SELECT 'limit', * FROM remote('127.1', view(SELECT * FROM numbers(10))) SETTINGS limit=5
