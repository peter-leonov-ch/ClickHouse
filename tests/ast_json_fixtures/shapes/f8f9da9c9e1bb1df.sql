SELECT
    id,
    t.*,
    toTypeName(t)
FROM qualified_match_nullable_tuple_direct
ORDER BY id
