SELECT
    number
FROM numbers(10)
GROUP BY
    GROUPING SETS (
        (number),
        (number % 2)
    )
ORDER BY number, grouping(number, number % 2) = 1
SETTINGS force_grouping_standard_compatibility=0
