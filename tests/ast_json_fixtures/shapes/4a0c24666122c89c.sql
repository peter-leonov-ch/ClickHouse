INSERT INTO TABLE FUNCTION remote('127.0.0.1', currentDatabase(), 'simple') SETTINGS prefer_localhost_replica=0, deduplicate_insert='enable' VALUES (1)
