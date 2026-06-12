WITH arrayMap(x -> (x + 1), [0]) AS a
SELECT 1
WHERE 1 IN (
    SELECT arrayJoin(a)
)
