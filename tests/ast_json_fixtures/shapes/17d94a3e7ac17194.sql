WITH
    (SELECT [0, 1, 2, 3]) AS arr1
SELECT arraySort(arrayIntersect(argMax(seqs, create_time), arr1)) AS common, id
FROM tags
WHERE id LIKE 'id%'
GROUP BY id
ORDER BY id
