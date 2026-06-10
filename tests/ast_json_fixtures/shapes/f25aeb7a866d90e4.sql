SELECT
    A.id AS a_id,
    (
        SELECT groupArraySorted(5)(B.id)
        FROM B
        WHERE has(B.A_ids, A.id)
    ) AS b_ids_containing_a_id
FROM A
ORDER BY a_id
LIMIT 20
