CREATE MATERIALIZED VIEW 03789_rmv_mv REFRESH EVERY 1 MONTH APPEND TO 03789_rmv_target AS WITH
    (
        SELECT 1
    ) AS lower_limit,
    (
        SELECT number
        FROM numbers(10)
        WHERE number = lower_limit
    ) AS upper_limit,
    result AS
    (
        SELECT 'OH NO' AS message
        FROM numbers(10)
        WHERE (number >= lower_limit) AND (number <= upper_limit)
    )
SELECT *
FROM result
