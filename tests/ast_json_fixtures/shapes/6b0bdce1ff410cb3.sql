SELECT
    number,
    grouping(number, number % 2) = 3
FROM remote('127.0.0.{2,3}', numbers(10))
GROUP BY
    number,
    number % 2
ORDER BY number
SETTINGS force_grouping_standard_compatibility=0
