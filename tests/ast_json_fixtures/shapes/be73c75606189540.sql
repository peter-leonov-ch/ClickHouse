WITH
    a AS ( SELECT 0 AS key, 'a' AS acol ),
    b AS ( SELECT 2 AS key )
SELECT a.acol, a1.acol
FROM b
FULL JOIN a ON a.key = b.key
FULL JOIN a AS a1 ON a1.key = a.key
ORDER BY 1
