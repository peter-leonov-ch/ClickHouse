SELECT color_id, payload
FROM t
ORDER BY color_id, payload
LIMIT 1 BY (dictGetString('colors','name', color_id) = 'red')
