SELECT dummy
FROM remote('127.{1,2}', system.one)
WHERE dummy IN (SELECT 0)
LIMIT 1 BY dummy
