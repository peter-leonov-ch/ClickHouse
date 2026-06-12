SELECT fact_3_id
FROM grouping_sets
GROUP BY
    GROUPING SETS ((fact_3_id, fact_4_id))
ORDER BY fact_3_id ASC
