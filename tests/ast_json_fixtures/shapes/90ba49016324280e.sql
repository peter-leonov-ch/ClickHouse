SELECT
    fact_1_id,
    fact_3_id,
    SUM(sales_value) AS sales_value
FROM grouping_sets
GROUP BY grouping sets (fact_1_id, (fact_1_id, fact_3_id)) WITH TOTALS
ORDER BY fact_1_id, fact_3_id
