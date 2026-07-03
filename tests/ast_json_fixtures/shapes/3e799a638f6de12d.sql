SELECT name, count() AS cnt
FROM remote('127.{1,2}', system.settings)
GROUP BY name
HAVING (max(value) > '9') AND (min(changed) = 0)
FORMAT Null
