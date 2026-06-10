WITH
    a as b
SELECT 1 FROM (
    WITH b as c 
    SELECT 1 FROM (
        WITH c as d
        SELECT 1 FROM (
            SELECT 1 FROM tab WHERE e = 'true'
        )
    )
)
SETTINGS allow_experimental_analyzer = 0
