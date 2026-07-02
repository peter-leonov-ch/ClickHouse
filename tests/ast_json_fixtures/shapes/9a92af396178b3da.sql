SELECT db.name, t.name
    FROM (SELECT name, database FROM system.tables WHERE name = 'one') AS t
    JOIN (SELECT name FROM system.databases WHERE name = 'system') AS db ON t.database = db.name
    FORMAT PrettyCompactNoEscapes
