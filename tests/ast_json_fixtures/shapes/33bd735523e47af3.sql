WITH RECURSIVE subdepartment AS
(
    
    SELECT 1 AS level, * FROM department WHERE name = 'A'

    UNION ALL

    
    SELECT sd.level + 1, d.* FROM department AS d, subdepartment AS sd
        WHERE d.parent_department = sd.id
)
SELECT * FROM subdepartment WHERE level >= 2 ORDER BY name
