SELECT x, t.name
    FROM (SELECT name, database AS x FROM system.tables) AS t
    JOIN (SELECT name AS x FROM system.databases) AS db USING x
    WHERE x = 'system' AND t.name = 'one'
    SETTINGS join_default_strictness = 'ALL'
    FORMAT PrettyCompactNoEscapes
