DELETE FROM test_deletes WHERE a >= 100 AND a < 200 SETTINGS lightweight_deletes_sync = 1
