SELECT
    number
FROM numbers(10)
GROUP BY
    GROUPING SETS (
        (number),
        (number % 2)
    )
HAVING grouping(number, number % 2) = 1
ORDER BY number
SETTINGS enable_optimize_predicate_expression = 0, force_grouping_standard_compatibility=0
