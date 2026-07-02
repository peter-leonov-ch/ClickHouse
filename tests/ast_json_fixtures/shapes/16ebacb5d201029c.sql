SELECT u1.uid, u1.spouse_name as name, u2.uid, u2.name
FROM users u1 JOIN users u2 USING (name)
ORDER BY u1.uid
FORMAT TSVWithNamesAndTypes
SETTINGS enable_analyzer = 0
