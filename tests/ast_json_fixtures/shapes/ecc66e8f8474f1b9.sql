SELECT
    a,
    b,
    cramersVBiasCorrected(a, b)
FROM numbers(3)
GROUP BY
    number AS a,
    number + number AS b
    WITH CUBE
SETTINGS group_by_use_nulls = 1
