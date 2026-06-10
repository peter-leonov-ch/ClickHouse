SELECT 'w\0\0ldworldwo\0l\0world'
FROM grouping_sets
GROUP BY
    GROUPING SETS (
    ( fact_4_id),
    ( NULL),
    ( fact_3_id, fact_4_id))
ORDER BY
    NULL ASC,
    NULL DESC NULLS FIRST,
    fact_3_id ASC,
    fact_3_id ASC NULLS LAST,
    'wo\0ldworldwo\0ldworld' ASC NULLS LAST,
    'w\0\0ldworldwo\0l\0world' DESC NULLS FIRST,
    'wo\0ldworldwo\0ldworld' ASC,
    NULL ASC NULLS FIRST,
    fact_4_id DESC NULLS LAST
