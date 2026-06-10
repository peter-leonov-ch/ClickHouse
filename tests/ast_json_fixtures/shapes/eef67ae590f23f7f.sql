SELECT fact_3_id, fact_4_id, count()
FROM grouping_sets
GROUP BY
    GROUPING SETS (
    ( fact_3_id, fact_4_id))
ORDER BY fact_3_id, fact_4_id
SETTINGS optimize_aggregation_in_order=1
