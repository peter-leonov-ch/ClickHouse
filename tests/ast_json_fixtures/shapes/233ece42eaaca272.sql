SELECT
    coalesce(qualified_match_join_left.x, qualified_match_join_right.x) AS x,
    t.*,
    toTypeName(t)
FROM qualified_match_join_left
FULL JOIN qualified_match_join_right USING (t)
ORDER BY x
