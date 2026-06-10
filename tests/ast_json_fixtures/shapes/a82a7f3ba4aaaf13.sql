SELECT
    arr.*,
    toTypeName(arr)
FROM
(
    SELECT
        [tuple(1, 'a'), NULL, tuple(3, 'c')]::Array(Nullable(Tuple(a Int32, s String))) AS arr
) AS src
